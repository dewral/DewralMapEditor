#include "luascriptengine.h"
#include "customgeneratorprofiles.h"
#include "custommaprecipe.h"
#include <QtConcurrent>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <cstdlib>
#include <limits>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace {
constexpr size_t memoryLimit = 256 * 1024 * 1024;
constexpr int tileLimit = 100000;
constexpr int itemLimit = 500000;
struct RunContext {
    size_t memory = 0;
    QString root;
    QString log;
    std::shared_ptr<std::atomic_bool> cancelled;
    QElapsedTimer timer;
    int timeoutMs = 120000;
};
void *allocate(void *ud, void *ptr, size_t oldSize, size_t newSize)
{
    auto *ctx = static_cast<RunContext *>(ud);
    if (!ptr) oldSize = 0;
    if (!newSize) { std::free(ptr); ctx->memory -= oldSize; return nullptr; }
    if (newSize > memoryLimit - (ctx->memory - oldSize)) return nullptr;
    void *result = std::realloc(ptr, newSize);
    if (result) ctx->memory = ctx->memory - oldSize + newSize;
    return result;
}
RunContext *context(lua_State *L)
{
    void *ud = nullptr;
    lua_getallocf(L, &ud);
    return static_cast<RunContext *>(ud);
}
void checkBudget(lua_State *L, lua_Debug *)
{
    const auto *ctx = context(L);
    if (ctx->cancelled->load()) luaL_error(L, "Generation cancelled.");
    if (ctx->timer.elapsed() > ctx->timeoutMs) luaL_error(L, "Generation time limit exceeded.");
}
int printLog(lua_State *L)
{
    auto *ctx = context(L);
    if (ctx->log.size() >= 32768) return 0;
    for (int i = 1; i <= lua_gettop(L); ++i) {
        if (i > 1) ctx->log += QLatin1Char('\t');
        const char *s = lua_tostring(L, i);
        ctx->log += s ? QString::fromUtf8(s) : QString::fromLatin1(luaL_typename(L, i));
    }
    ctx->log += QLatin1Char('\n');
    return 0;
}
int listFiles(lua_State *L)
{
    const QString folder = QString::fromUtf8(luaL_checkstring(L, 1));
    const QStringList names = QDir(context(L)->root + folder).entryList(
        {QStringLiteral("*.lua")}, QDir::Files, QDir::Name);
    lua_createtable(L, names.size(), 0);
    int n = 0;
    for (const QString &name : names) {
        const QByteArray utf8 = name.toUtf8();
        lua_pushlstring(L, utf8.constData(), utf8.size());
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}
bool runChunk(lua_State *L, const QByteArray &code, const char *name, QString &error)
{
    if (luaL_loadbuffer(L, code.constData(), code.size(), name) || lua_pcall(L, 0, 0, 0)) {
        error = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}
// Result buffers contain data only. Never execute script metamethods while
// converting them to Qt values outside the protected script call.
void rawField(lua_State *L, int index, const char *field)
{
    if (index < 0 && index > LUA_REGISTRYINDEX) index = lua_gettop(L) + index + 1;
    lua_pushstring(L, field);
    lua_rawget(L, index);
}
bool integerField(lua_State *L, int index, const char *field, int low, int high, int &out)
{
    rawField(L, index, field);
    const double value = lua_tonumber(L, -1);
    const bool valid = lua_type(L, -1) == LUA_TNUMBER && value >= low && value <= high
                       && value == static_cast<int>(value);
    if (valid) out = static_cast<int>(value);
    lua_pop(L, 1);
    return valid;
}
void pushVariant(lua_State *L,const QVariant &value)
{
    if (value.metaType().id()==QMetaType::QVariantMap) {
        lua_newtable(L);
        const auto values=value.toMap();
        for (auto it=values.cbegin();it!=values.cend();++it) {
            const QByteArray key=it.key().toUtf8();
            pushVariant(L,it.value()); lua_setfield(L,-2,key.constData());
        }
    } else if (value.metaType().id()==QMetaType::QVariantList) {
        lua_newtable(L); int i=0;
        for (const QVariant &v : value.toList()) { pushVariant(L,v); lua_rawseti(L,-2,++i); }
    } else if (value.metaType().id()==QMetaType::Bool) lua_pushboolean(L,value.toBool());
    else if (value.metaType().id()==QMetaType::QString) {
        const QByteArray text=value.toString().toUtf8(); lua_pushlstring(L,text.constData(),text.size());
    } else if (value.isNull()) lua_pushnil(L);
    else lua_pushnumber(L,value.toDouble());
}
const char *api = R"lua(
local originalDofile, originalLoadfile = dofile, loadfile
local function resolve(path)
    if path:sub(1,1) == '/' or path:match('^%a:') then return path end
    return ROOT_PATH .. path
end
function dofile(path) return originalDofile(resolve(path)) end
function loadfile(path) return originalLoadfile(resolve(path)) end
package.path = ROOT_PATH .. '?.lua;' .. ROOT_PATH .. '?/init.lua;' .. package.path
package.loaders[3], package.loaders[4] = nil, nil
table.insert(package.loaders, 2, function(name)
    if name:find('/',1,true) or name:find('\\',1,true) then
        local fn,err = originalLoadfile(resolve(name .. '.lua'))
        return fn or err
    end
    return '\n\tno relative path module'
end)
-- Scripts need file reads, but generation does not need subprocesses or native modules.
os.execute, os.exit, io.popen, package.loadlib = nil, nil, nil, nil
local originalOpen = io.open
io.open = function(path, mode) return originalOpen(resolve(path), mode) end
_dme_tiles = {}
dme = { apiVersion = 1 }
local function tile(pos)
    assert(type(pos) == 'table', 'Expected a position table')
    local x,y,z = pos.x,pos.y,pos.z
    assert(type(x)=='number' and x==math.floor(x) and x>=0 and x<=65535, 'Invalid x')
    assert(type(y)=='number' and y==math.floor(y) and y>=0 and y<=65535, 'Invalid y')
    assert(type(z)=='number' and z==math.floor(z) and z>=0 and z<=15, 'Invalid floor')
    local key = x .. ':' .. y .. ':' .. z
    if not _dme_tiles[key] then _dme_tiles[key] = {x=x,y=y,z=z,items={}} end
    return _dme_tiles[key]
end
local function item(id, count)
    assert(type(id)=='number' and id==math.floor(id) and id>=1 and id<=65535, 'Invalid item ID')
    count = count or 1
    assert(type(count)=='number' and count==math.floor(count) and count>=0 and count<=255, 'Invalid count (0–255)')
    return {id=id,count=count}
end
function dme.setGround(pos,id) tile(pos).ground = item(id,1) end
function dme.addItem(pos,id,count) table.insert(tile(pos).items,item(id,count)) end
function dme.clearTile(pos) local t=tile(pos); t.ground=nil; t.items={} end
function dme.getTile(pos) return tile(pos) end -- Detached generation buffer, never the live map.
function dme.log(message) print(message) end
math.randomseed(DME_SEED)
)lua";
const char *generator = R"lua(
RUNNING_MODE = 'cli'
PRECREATION_TABLE_MODE = true
RETURNVALUE_NOERROR = 0
logger = setmetatable({}, {__index=function() return function() end end})
dofile('config.lua')
local folders = {'lib/externalResources','lib','lib/classes/data','lib/classes/data/reader',
 'lib/classes/data/saver','lib/classes/tools','lib/classes','lib/data','lib/data/borders',
 'lib/data/brushes','lib/data/elevation','lib/data/shapes','lib/commonScripts'}
for _,folder in ipairs(folders) do
    for _,name in ipairs(_dme_listFiles(folder)) do dofile(folder .. '/' .. name) end
end
-- Upstream mockFunctions reseeds using the clock; override it for repeatable runs.
math.randomseed(DME_SEED)
LOG_TO_FILE, DEBUG_OUTPUT = false, false
local script = dofile(DME_SCRIPT)
LOG_TO_FILE, DEBUG_OUTPUT = false, false
assert(type(script)=='table' and type(script.run)=='function', 'Not an otMapGen generation script')
script.run()
assert(type(CLI_FINAL_MAP_TABLE)=='table', 'Generator returned no map table')
local origin = MAP_CONFIGURATION.mainPos
_dme_origin = {x=origin.x,y=origin.y,z=origin.z}
for x,ys in pairs(CLI_FINAL_MAP_TABLE) do
 for y,zs in pairs(ys) do
  for z,stack in pairs(zs) do
   local positions = {}
   for index,value in pairs(stack) do
    if type(value)=='table' and value.itemid and value.itemid>0 then positions[#positions+1]=index end
   end
   table.sort(positions)
   if #positions>0 then
    local pos={x=x,y=y,z=z}
    dme.clearTile(pos)
    for _,index in ipairs(positions) do
     local value=stack[index]
     -- Generator handles are transient. They are not OTBM unique IDs.
     if isGround(value.itemid) then dme.setGround(pos,value.itemid)
     else dme.addItem(pos,value.itemid,value.typeOrCount) end
    end
   end
  end
 end
end
)lua";
}

LuaScriptEngine::LuaScriptEngine(QObject *parent) : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<QVariantMap>::finished, this, [this] {
        m_busy = false;
        emit busyChanged();
        emit finished(m_watcher.result());
    });
}
LuaScriptEngine::~LuaScriptEngine() { cancel(); m_watcher.waitForFinished(); }
void LuaScriptEngine::cancel() { if (m_cancelled) m_cancelled->store(true); }
void LuaScriptEngine::run(const QString &path, const QString &mode, int seed)
{
    if (m_busy) return;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    m_busy = true;
    emit busyChanged();
    m_watcher.setFuture(QtConcurrent::run([path, mode, seed, flag=m_cancelled] {
        return execute(path, mode, seed, flag);
    }));
}
void LuaScriptEngine::runProfile(const QVariantMap &profile,const QVariantMap &itemInfo)
{
    if (m_busy) return;
    auto configuration=profile;
    configuration["itemInfo"]=itemInfo;
    m_cancelled=std::make_shared<std::atomic_bool>(false);
    m_busy=true; emit busyChanged();
    m_watcher.setFuture(QtConcurrent::run([configuration,flag=m_cancelled] {
        return execute(QString(),"profile",configuration.value("seed").toInt(),flag,120000,configuration);
    }));
}
QStringList LuaScriptEngine::generatorScripts(const QString &directory) const
{
    return QDir(directory + QStringLiteral("/data/genScripts")).entryList(
        {QStringLiteral("*.lua")}, QDir::Files, QDir::Name);
}
QVariantMap LuaScriptEngine::execute(const QString &path, const QString &mode, int seed,
                                   const std::shared_ptr<std::atomic_bool> &cancelled, int timeoutMs,
                                   const QVariantMap &profile)
{
    QVariantMap result{{QStringLiteral("success"), false}};
    RunContext ctx;
    ctx.cancelled = cancelled;
    ctx.timeoutMs = timeoutMs;
    ctx.timer.start();
    QFileInfo scriptFile(path);
    const bool otMapGen = mode == QLatin1String("otmapgen");
    const bool customProfile = mode == QLatin1String("profile");
    if (customProfile) {
        const auto info=profile.value("itemInfo").toMap();
        const QString error=CustomGeneratorProfiles::validate(profile,&info);
        if (!error.isEmpty()) { result.insert("error",error); return result; }
    }
    QDir root(scriptFile.absolutePath());
    if (otMapGen) {
        if (root.dirName() != QLatin1String("genScripts") || !root.cdUp() || !root.cdUp()) {
            result.insert("error", "Select a script inside otMapGenPublic/data/genScripts.");
            return result;
        }
    } else if (mode != QLatin1String("script") && !customProfile) {
        result.insert("error", "Unknown Lua execution mode.");
        return result;
    }
    ctx.root = QDir::fromNativeSeparators(root.absolutePath()) + QLatin1Char('/');
    lua_State *L = lua_newstate(allocate, &ctx);
    if (!L) { result.insert("error", "Could not allocate Lua VM."); return result; }
    luaL_openlibs(L);
    lua_sethook(L, checkBudget, LUA_MASKCOUNT, 10000);
    lua_pushcfunction(L, printLog); lua_setglobal(L, "print");
    lua_pushcfunction(L, listFiles); lua_setglobal(L, "_dme_listFiles");
    const QByteArray rootUtf8 = ctx.root.toUtf8();
    lua_pushlstring(L, rootUtf8.constData(), rootUtf8.size()); lua_setglobal(L, "ROOT_PATH");
    lua_pushinteger(L, seed); lua_setglobal(L, "DME_SEED");
    if (customProfile) { pushVariant(L,profile); lua_setglobal(L,"DME_PROFILE"); }
    const QByteArray scriptName = (otMapGen ? QStringLiteral("data/genScripts/") + scriptFile.fileName()
                                          : scriptFile.fileName()).toUtf8();
    lua_pushlstring(L, scriptName.constData(), scriptName.size()); lua_setglobal(L, "DME_SCRIPT");
    QString error;
    bool ok = runChunk(L, api, "@dme-api", error);
    if (ok) {
        if (customProfile) {
            if (ok) ok=runChunk(L,customMapRecipe,"@custom-map-recipe",error);
        } else if (otMapGen) ok = runChunk(L, generator, "@otmapgen-adapter", error);
        else ok = runChunk(L, "dofile(DME_SCRIPT)", "@dme-script", error);
    }
    QVariantList tiles;
    int itemCount = 0, originX = 65535, originY = 65535, originZ = 0;
    if (ok) {
        rawField(L, LUA_GLOBALSINDEX, "_dme_tiles");
        if (!lua_istable(L, -1)) { ok = false; error = "Script overwrote the generation buffer."; }
        if (ok) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                int x=0,y=0,z=0;
                if (!lua_istable(L, -1) || !integerField(L,lua_gettop(L),"x",0,65535,x)
                    || !integerField(L,lua_gettop(L),"y",0,65535,y)
                    || !integerField(L,lua_gettop(L),"z",0,15,z)) {
                    ok=false; error="Invalid tile in the generation buffer."; break;
                }
                QVariantList items;
                auto readItem = [&](int index, bool ground) {
                    int id=0,count=0;
                    if (!lua_istable(L,index) || !integerField(L,index,"id",1,65535,id)
                        || !integerField(L,index,"count",0,255,count)) return false;
                    items.append(QVariantMap{{"id",id},{"count",count},{"ground",ground}});
                    return true;
                };
                rawField(L,-1,"ground");
                if (!lua_isnil(L,-1) && !readItem(lua_gettop(L),true)) ok=false;
                lua_pop(L,1);
                rawField(L,-1,"items");
                if (!lua_istable(L,-1)) ok=false;
                if (ok) {
                    const size_t count=lua_objlen(L,-1);
                    if (count > 1024) ok=false;
                    for (size_t i=1; ok && i<=count; ++i) {
                        lua_rawgeti(L,-1,static_cast<int>(i));
                        ok=readItem(lua_gettop(L),false);
                        lua_pop(L,1);
                    }
                }
                lua_pop(L,1);
                itemCount += items.size();
                if (!ok || tiles.size() >= tileLimit || itemCount > itemLimit) {
                    ok=false; error="Invalid item or generation size limit exceeded."; break;
                }
                tiles.append(QVariantMap{{"x",x},{"y",y},{"z",z},{"items",items}});
                originX=qMin(originX,x); originY=qMin(originY,y); originZ=qMax(originZ,z);
                lua_pop(L,1);
                if (cancelled->load() || ctx.timer.elapsed()>timeoutMs) {
                    ok=false; error="Generation cancelled or time limit exceeded."; break;
                }
            }
        }
    }
    if (ok && (otMapGen || customProfile)) {
        rawField(L, LUA_GLOBALSINDEX,"_dme_origin");
        const int index=lua_gettop(L);
        ok=lua_istable(L,index) && integerField(L,index,"x",0,65535,originX)
            && integerField(L,index,"y",0,65535,originY) && integerField(L,index,"z",0,15,originZ);
        if (!ok) error="Invalid generator origin.";
    }
    lua_close(L);
    if (ok && tiles.isEmpty()) { ok=false; error="Script produced no tiles."; }
    if (cancelled->load()) { ok=false; error="Generation cancelled."; }
    result.insert("success",ok);
    result.insert("log",ctx.log);
    result.insert("elapsedMs",ctx.timer.elapsed());
    if (!ok) result.insert("error",error);
    else {
        result.insert("tiles",tiles); result.insert("tileCount",tiles.size());
        result.insert("itemCount",itemCount); result.insert("originX",originX);
        result.insert("originY",originY); result.insert("originZ",originZ);
    }
    return result;
}
