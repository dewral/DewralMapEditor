#pragma once
// DME-owned recipes. Upstream libraries remain in the user-selected folder.
constexpr const char *customMapRecipe = R"lua(
local cfg=DME_PROFILE
local materials=cfg.materials
local meta=cfg.itemInfo
local bg,walk,second=80000,80001,80002
local markerRoles={[bg]='background',[walk]='floor',[second]='secondary'}
for i=1,4 do markerRoles[80100+i]='wall'..i end
for i=1,12 do markerRoles[80200+i]='border'..i end
local function chooseEntries(entries)
 local total=0
 for _,entry in ipairs(entries) do total=total+entry.weight end
 assert(total>0,'Missing material variants')
 local n=math.random(1,total)
 for _,entry in ipairs(entries) do n=n-entry.weight; if n<=0 then return entry.id end end
end
local function choose(role) return chooseEntries(materials[role] or {}) end
function isGround(id)
 return id==bg or id==walk or id==second or (meta[tostring(id)] and meta[tostring(id)].ground) or false
end
ITEMS_TABLE={[0]={bg},[1]={walk},[12]={second}}
local pos={x=32,y=32,z=7}
MAP_CONFIGURATION={mainPos=pos,mapSizeX=cfg.width-1,mapSizeY=cfg.height-1,mapSizeZ=1}
_dme_origin={x=pos.x,y=pos.y,z=pos.z}
local maxX,maxY=pos.x+cfg.width-1,pos.y+cfg.height-1
local originalCreate=doCreateItemMock
-- Some upstream brushes have a one-cell halo. Never export it beyond the requested rectangle.
doCreateItemMock=function(id,count,p,safe)
 if p.x<pos.x or p.x>maxX or p.y<pos.y or p.y>maxY then return end
 return originalCreate(id,count,p,safe)
end
local map
if cfg.kind=='cave' then map=CaveGroundMapper.new(pos,cfg.width-1,cfg.height-1,1,6)
else map=GroundMapper.new(pos,cfg.width-1,cfg.height-1,1,6) end
map:doMainGround(ITEMS_TABLE,7)
-- Bounded grid sampling avoids upstream's unbounded waypoint retries and factorial TSP.
local candidates={}
for y=pos.y+8,maxY-7,8 do
 for x=pos.x+8,maxX-7,8 do candidates[#candidates+1]={x=x,y=y,z=7} end
end
assert(#candidates>=cfg.rooms,'Too many rooms for this map size')
for i=#candidates,2,-1 do local j=math.random(i); candidates[i],candidates[j]=candidates[j],candidates[i] end
local points={[7]={}}
for i=1,cfg.rooms do points[7][i]={pos=candidates[i]} end
local cursor=Cursor.new(pos)
local paths
if cfg.kind=='cave' then
 paths=CaveWayPointer.new(map,cursor,points,Brush.new())
 CaveRoomBuilder.new(map,points[7]):createRooms(ITEMS_TABLE,7,7,7)
else
 paths=WayPointer.new(map,cursor,points)
 local shape={height=7,width=7,shape={}}
 for y=1,7 do shape.shape[y]={}; for x=1,7 do shape.shape[y][x]=1 end end
 DungeonRoomBuilder.new(map,points[7]):createRooms(ITEMS_TABLE,{shape},7)
end
local function copy(p) return {x=p.x,y=p.y,z=p.z} end
-- Join every room into one network using the original otMapGen brush/path builders.
for i=2,cfg.rooms do
 local a,b=copy(points[7][i-1].pos),copy(points[7][i].pos)
 if cfg.kind=='cave' then paths:_connectTwoPoints(ITEMS_TABLE,a,b,3)
 else paths:_createPathBetweenTwoPointsTSP(ITEMS_TABLE,a,b,3,7) end
end
if cfg.wallsEnabled then
 local walls={}; for i=1,4 do walls[i]={80100+i} end
 walls[5],walls[6]={},{}
 WallAutoBorder.new(map):doWalls(walk,bg,walls,7)
end
local function stackAt(x,y) return CLI_FINAL_MAP_TABLE[x][y][7] end
local function groundAt(x,y)
 local g=rawget(stackAt(x,y),1)
 return g and g.itemid or 0
end
local resolvedGrounds
local borderSets={}
for _,set in ipairs(cfg.groundBorders or {}) do
 local active=false
 for _,entries in pairs(set.materials) do if #entries>0 then active=true end end
 if active then borderSets[#borderSets+1]=set end
end
if cfg.bordersEnabled and #borderSets==0 then
 -- Version 1 profiles retain the original default room-floor border slots.
 local borders={}; for i=1,12 do borders[i]={80200+i} end
 GroundAutoBorder.new(map):doGround(walk,bg,0,borders,7)
elseif cfg.bordersEnabled then
 resolvedGrounds={}
 for y=pos.y,maxY do
  resolvedGrounds[y]={}
  for x=pos.x,maxX do
   local id=groundAt(x,y)
   if id==walk and #(materials.secondary or {})>0 and math.random(100)<=15 then id=second end
   local role=markerRoles[id]
   resolvedGrounds[y][x]=role and choose(role) or id
  end
 end
 local function idSet(ids)
  local result={}; for _,id in ipairs(ids) do result[id]=true end; return result
 end
 local assigned={}
 for _,set in ipairs(borderSets) do for _,id in ipairs(set.groundIds) do assigned[id]=true end end
 local defaults={}
 for _,entry in ipairs(materials.floor) do if not assigned[entry.id] then defaults[#defaults+1]=entry.id end end
 if #defaults>0 and #(materials.border1 or {})>0 then
  local target={}; for _,entry in ipairs(materials.background) do target[#target+1]=entry.id end
  borderSets[#borderSets+1]={groundIds=defaults,to='background',toGroundIds=target,align='outer',zorder=0,materials=materials}
 end
 -- Preserve brush stacking order; equal priorities use profile order.
 for i,set in ipairs(borderSets) do
  set._order=i; set._owners=idSet(set.groundIds); set._targets=idSet(set.toGroundIds)
  local identity={}; for _,id in ipairs(set.groundIds) do identity[#identity+1]=id end
  table.sort(identity); set._signature=table.concat(identity,',')..set.align
 end
 table.sort(borderSets,function(a,b) return a.zorder==b.zorder and a._order<b._order or a.zorder<b.zorder end)
 for _,set in ipairs(borderSets) do
  set._exclusions={}
  if set.to=='*' then for _,other in ipairs(borderSets) do
   if other.to~='*' and other._signature==set._signature then
    if other.to=='' then set._exclusions[0]=true end
    for id in pairs(other._targets) do set._exclusions[id]=true end
   end
  end end
 end
 local function actualGround(x,y) return resolvedGrounds[y] and resolvedGrounds[y][x] or 0 end
 local function targetMatches(set,id)
  if set.to~='*' then return (set.to=='' and id==0) or set._targets[id] end
  return not set._exclusions[id]
 end
 -- Same neighbour masks and corner rules as BrushStore::computeBorderItems.
 local borderTypes={
  0,5,1,1,6,1541,1,1,4,4,9,9,1540,1540,9,9,
  2,517,10,10,2,517,10,10,516,516,132097,132097,1026,1026,262657,262657,
  7,1287,263,263,1543,329223,263,263,4,4,9,9,1540,1540,9,9,
  519,328199,2567,2567,519,328199,2567,2567,516,516,66052,66052,516,516,66052,66052,
  3,1283,259,259,1539,329219,259,259,12,12,262403,262403,1548,1548,262403,262403,
  11,1291,131331,131331,11,1291,131331,131331,132099,132099,16909315,16909315,132099,132099,16909315,16909315,
  3,1283,259,259,1539,394499,259,259,12,12,66563,66563,1548,1548,66563,66563,
  11,1291,66051,66051,11,1291,66051,66051,262659,262659,67174915,67174915,262659,262659,67174915,67174915,
  8,2053,2049,2049,2054,525574,2049,2049,2052,2052,2057,2057,525828,525828,2057,2057,
  2,1282,10,10,2,1282,10,10,1026,1026,262657,66562,1026,1026,262657,262657,
  2055,525575,524551,524551,525831,134546951,524551,524551,2052,2052,2057,2057,525828,525828,2057,2057,
  519,328199,2567,2567,519,328199,2567,2567,516,516,66052,66052,516,516,66052,66052,
  3,1283,259,259,1539,329219,259,259,12,12,262403,262403,1548,1548,262403,262403,
  11,1291,131331,131331,11,1291,131331,131331,132099,132099,16909315,16909315,132099,132099,16909315,16909315,
  3,1283,259,259,1539,394499,259,259,12,12,66563,66563,1548,1548,66563,66563,
  11,1291,66051,66051,11,1291,66051,66051,262659,262659,67174915,67174915,262659,262659,67174915,67174915
 }
 local offsets={{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}
 local slots={3,4,1,2,8,7,6,5,11,12,10,9}
 local cornerFallbacks={[9]={4,1},[10]={2,1},[11]={3,2},[12]={3,4}}
 local function addBorder(set,direction,p)
  local entries=set.materials['border'..slots[direction]] or {}
  if #entries>0 then doCreateItemMock(chooseEntries(entries),1,p)
  elseif cornerFallbacks[direction] then
   for _,side in ipairs(cornerFallbacks[direction]) do addBorder(set,side,p) end
  end
 end
 for y=pos.y,maxY do for x=pos.x,maxX do
  local center=actualGround(x,y)
  for _,set in ipairs(borderSets) do
   local inside=set._owners[center]
   if (set.align=='inner' and inside) or (set.align=='outer' and not inside and targetMatches(set,center)) then
    local mask=0
    for i,offset in ipairs(offsets) do
     local neighbour=actualGround(x+offset[1],y+offset[2])
     local matches=set.align=='outer' and set._owners[neighbour]
       or (set.align=='inner' and not set._owners[neighbour] and targetMatches(set,neighbour))
     if matches then mask=mask+2^(i-1) end
    end
    if mask>0 then
     local packed=borderTypes[mask+1]
     for i=1,4 do
      local direction=packed%256; if direction==0 then break end
      addBorder(set,direction,{x=x,y=y,z=7}); packed=math.floor(packed/256)
     end
    end
   end
  end
 end end
end
local function blocks(id)
 local role=markerRoles[id]
 if role then
  -- A slot is treated as blocking if any of its possible variants blocks.
  for _,e in ipairs(materials[role] or {}) do if meta[tostring(e.id)].blocked then return true end end
  return false
 end
 return meta[tostring(id)] and meta[tostring(id)].blocked or false
end
local cells,passable={},{}
local function key(x,y) return (y-pos.y)*cfg.width+(x-pos.x)+1 end
for y=pos.y,maxY do for x=pos.x,maxX do
 local g=groundAt(x,y)
 if g==walk then
  local blocked=false
  for _,v in pairs(stackAt(x,y)) do if v.itemid and blocks(v.itemid) then blocked=true end end
  if not blocked then local k=key(x,y); cells[#cells+1]={x=x,y=y,k=k}; passable[k]=true end
 end
end end
local function connected(excluded)
 local start=nil; local total=0
 for _,c in ipairs(cells) do if passable[c.k] and c.k~=excluded then total=total+1; start=start or c end end
 if not start then return false end
 local seen={[start.k]=true}; local queue={start}; local head=1
 while head<=#queue do
  local c=queue[head]; head=head+1
  for _,d in ipairs({{1,0},{-1,0},{0,1},{0,-1}}) do
   local nx,ny=c.x+d[1],c.y+d[2]
   if nx>=pos.x and nx<=maxX and ny>=pos.y and ny<=maxY then
    local k=key(nx,ny)
    if k~=excluded and passable[k] and not seen[k] then
     seen[k]=true; queue[#queue+1]={x=nx,y=ny,k=k}
    end
   end
  end
 end
 return #queue==total,seen
end
local valid,reachable=connected()
assert(valid,'The layout or blocking wall/border materials disconnect the walkable area. Change materials or seed.')
for _,p in ipairs(points[7]) do assert(reachable[key(p.pos.x,p.pos.y)],'A room is blocked. Change materials or seed.') end
local roomCenters={}; for _,p in ipairs(points[7]) do roomCenters[key(p.pos.x,p.pos.y)]=true end
if #(materials.decorations or {})>0 and cfg.density>0 then
 for i=#cells,2,-1 do local j=math.random(i); cells[i],cells[j]=cells[j],cells[i] end
 local budget=math.floor(#cells*cfg.density/100)
 local placed,blockingChecks=0,0
 for _,c in ipairs(cells) do
  if placed>=budget then break end
  if not roomCenters[c.k] then
   local id=choose('decorations')
   local canPlace=true
   if blocks(id) then
    blockingChecks=blockingChecks+1
    canPlace=blockingChecks<=256 and connected(c.k)
   end
   if canPlace then
    doCreateItemMock(id,1,{x=c.x,y=c.y,z=7})
    if blocks(id) then passable[c.k]=nil end
    placed=placed+1
   end
  end
 end
 print('Decorations placed: '..placed)
end
assert(connected(),'Decoration placement disconnected the map')
-- Emit rows in a stable order and use no IDs outside the selected material pools.
for y=pos.y,maxY do for x=pos.x,maxX do
 local p={x=x,y=y,z=7}; local stack=stackAt(x,y)
 dme.clearTile(p)
 local indices={}; for index,v in pairs(stack) do if v.itemid then indices[#indices+1]=index end end
 table.sort(indices)
 for _,index in ipairs(indices) do
  local v=stack[index]; local id=v.itemid; local ground=isGround(id)
  if ground and resolvedGrounds then id=resolvedGrounds[y][x]
  else
   if id==walk and #(materials.secondary or {})>0 and math.random(100)<=15 then id=second end
   local role=markerRoles[id]
   if role then id=choose(role) end
  end
  if ground then dme.setGround(p,id) else dme.addItem(p,id,v.typeOrCount or 1) end
 end
end end
print('Custom '..cfg.kind..': '..cfg.width..' x '..cfg.height..', '..cfg.rooms..' connected rooms')
)lua";
