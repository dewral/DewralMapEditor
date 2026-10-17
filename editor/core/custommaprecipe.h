#pragma once
// Standalone DME Lua generator, embedded for distribution without external files.
constexpr const char *customMapRecipe = R"lua(
local cfg=DME_PROFILE
local materials=cfg.materials
local meta=cfg.itemInfo
local bg,walk,second=80000,80001,80002
local markerRoles={[bg]='background',[walk]='floor',[second]='secondary'}
for i=1,4 do markerRoles[80100+i]='wall'..i end
for i=1,12 do markerRoles[80200+i]='border'..i end
local function choose(role)
 local entries=materials[role] or {}
 local total=0
 for _,entry in ipairs(entries) do total=total+entry.weight end
 assert(total>0,'Missing material: '..role)
 local n=math.random(1,total)
 for _,entry in ipairs(entries) do n=n-entry.weight; if n<=0 then return entry.id end end
end
local pos={x=0,y=0,z=7}
_dme_origin=pos
local maxX,maxY=cfg.width-1,cfg.height-1
local layout,stacks={},{}
local function key(x,y) return y*cfg.width+x+1 end
local function floorAt(x,y)
 return x>=0 and y>=0 and x<=maxX and y<=maxY and layout[key(x,y)] or false
end
local function carve(x,y)
 if x>=2 and y>=2 and x<=maxX-2 and y<=maxY-2 then layout[key(x,y)]=true end
end
local function disk(x,y,r)
 for dy=-r,r do for dx=-r,r do
  if dx*dx+dy*dy<=r*r then carve(x+dx,y+dy) end
 end end
end
local candidates={}
for y=8,maxY-7,8 do for x=8,maxX-7,8 do candidates[#candidates+1]={x=x,y=y,z=7} end end
assert(#candidates>=cfg.rooms,'Too many rooms for this map size')
for i=#candidates,2,-1 do local j=math.random(i); candidates[i],candidates[j]=candidates[j],candidates[i] end
local points={[7]={}}
for i=1,cfg.rooms do
 local p=candidates[i]; points[7][i]={pos=p}
 if cfg.kind=='dungeon' then
  local rx,ry=math.random(2,3),math.random(2,3)
  for y=p.y-ry,p.y+ry do for x=p.x-rx,p.x+rx do carve(x,y) end end
 else
  disk(p.x,p.y,3)
  -- Connected overlapping lobes produce irregular cave chambers.
  for lobe=1,5 do disk(p.x+math.random(-2,2),p.y+math.random(-2,2),math.random(2,3)) end
 end
end
local function tunnel(a,b)
 local x,y=a.x,a.y; local horizontal=math.random(2)==1
 while x~=b.x or y~=b.y do
  if cfg.kind=='cave' then disk(x,y,math.random(1,2))
  else for dy=-1,1 do for dx=-1,1 do carve(x+dx,y+dy) end end end
  local moveX=x~=b.x and (y==b.y or (cfg.kind=='cave' and math.random(2)==1) or (cfg.kind=='dungeon' and horizontal))
  if moveX then x=x+(b.x>x and 1 or -1) else y=y+(b.y>y and 1 or -1) end
 end
 disk(x,y,1)
end
-- Attach every chamber to the nearest chamber already in the network.
for i=2,cfg.rooms do
 local a=points[7][i].pos; local nearest,distance=nil,math.huge
 for j=1,i-1 do
  local b=points[7][j].pos; local d=(a.x-b.x)^2+(a.y-b.y)^2
  if d<distance then nearest,distance=b,d end
 end
 tunnel(a,nearest)
end
local function stackAt(x,y) return stacks[key(x,y)] end
local function groundAt(x,y) return stackAt(x,y)[1].itemid end
local function isGround(id) return id==bg or id==walk or id==second end
local function addGeneratedItem(id,count,p)
 local stack=stackAt(p.x,p.y); stack[#stack+1]={itemid=id,typeOrCount=count}
end
for y=0,maxY do for x=0,maxX do stacks[key(x,y)]={{itemid=floorAt(x,y) and walk or bg}} end end
local function addRole(x,y,id) addGeneratedItem(id,1,{x=x,y=y,z=7}) end
for y=0,maxY do for x=0,maxX do
 local n,e,s,w=floorAt(x,y-1),floorAt(x+1,y),floorAt(x,y+1),floorAt(x-1,y)
 if cfg.wallsEnabled and not floorAt(x,y) then
  -- Place walls on background tiles to preserve corridor width.
  local count=(n and 1 or 0)+(e and 1 or 0)+(s and 1 or 0)+(w and 1 or 0)
  if count>0 then
   local slot
   if count>=3 then slot=3 elseif (n or s) and (e or w) then slot=4
   elseif n or s then slot=1 else slot=2 end
   addRole(x,y,80100+slot)
  elseif floorAt(x-1,y-1) or floorAt(x+1,y-1) or floorAt(x-1,y+1) or floorAt(x+1,y+1) then
   addRole(x,y,80104)
  end
 end
 if cfg.bordersEnabled and floorAt(x,y) then
  if not s then addRole(x,y,80201) end
  if not w then addRole(x,y,80202) end
  if not n then addRole(x,y,80203) end
  if not e then addRole(x,y,80204) end
  if not s and not e then addRole(x,y,80205) end
  if not s and not w then addRole(x,y,80206) end
  if not n and not e then addRole(x,y,80207) end
  if not n and not w then addRole(x,y,80208) end
  if s and w and not floorAt(x-1,y+1) then addRole(x,y,80209) end
  if s and e and not floorAt(x+1,y+1) then addRole(x,y,80210) end
  if n and w and not floorAt(x-1,y-1) then addRole(x,y,80211) end
  if n and e and not floorAt(x+1,y-1) then addRole(x,y,80212) end
 end
end end
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
    addGeneratedItem(id,1,{x=c.x,y=c.y,z=7})
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
  if id==walk and #(materials.secondary or {})>0 and math.random(100)<=15 then id=second end
  local role=markerRoles[id]
  if role then id=choose(role) end
  if ground then dme.setGround(p,id) else dme.addItem(p,id,v.typeOrCount or 1) end
 end
end end
print('Custom '..cfg.kind..': '..cfg.width..' x '..cfg.height..', '..cfg.rooms..' connected rooms')
)lua";
