--[[
   Some sort of stellar wind type background.
--]]
local bgshaders = require "bkg.lib.bgshaders"
local love_shaders = require 'love_shaders'
local graphics = require "love.graphics"
local lf = require 'love.filesystem'
local prng_lib = require "prng"
local prng = prng_lib.new()

local starfield = {}

starfield.stars = {
   "blue01.webp",
   "blue02.webp",
   "blue04.webp",
   "green01.webp",
   "green02.webp",
   "orange01.webp",
   "orange02.webp",
   "orange05.webp",
   "redgiant01.webp",
   "white01.webp",
   "white02.webp",
   "yellow01.webp",
   "yellow02.webp"
}

local starfield_frag = lf.read('bkg/shaders/starfield.frag')

local shader, sstarfield, sf, sz, sb

local function star_add( added, num_added )
   -- Set up parameters
   local path  = "gfx/bkg/star/"
   -- Avoid repeating stars
   local stars = starfield.stars
   local cur_sys = system.cur()
   local num   = prng:random(1,#stars)
   local i     = 0
   while added[num] and i < 10 do
      num = prng:random(1,#stars)
      i   = i + 1
   end
   local star  = stars[ num ]
   -- Load and set stuff
   local img   = tex.open( path .. star )
   -- Position should depend on whether there's more than a star in the system
   local r     = prng:random() * cur_sys:radius()/3
   if num_added > 0 then
      r        = r + cur_sys:radius()*2/3
   end
   local a     = 2*math.pi*prng:random()
   local x     = r*math.cos(a)
   local y     = r*math.sin(a)
   local nmove = math.max( 0.05, prng:random()*0.1 )
   local move  = 0.02 + nmove
   local scale = 1.0 - (1 - nmove/0.2)/5
   bkg.image( img, x, y, move, scale ) -- On the background
   return num
end

local function add_local_stars ()
   -- Chose number to generate
   local n
   local r = prng:random()
   if r > 0.97 then
      n = 3
   elseif r > 0.94 then
      n = 2
   elseif r > 0.1 then
      n = 1
   end

   -- If there is an inhabited planet we'll need at least one star
   if not n then
      for _k,v in ipairs( system.cur():planets() ) do
         if v:services().land then
            n = 1
            break
         end
      end
   end

   -- Generate the stars
   local i = 0
   local added = {}
   while n and i < n do
      num = star_add( added, i )
      added[ num ] = true
      i = i + 1
   end
end

function starfield.init( params )
   params = params or {}

   -- Scale factor that controls computation cost. As this shader is really
   -- really expensive, we can't compute it at full resolution
   sf = math.max( 1.0, naev.conf().nebu_scale * 0.5 )

   -- Per system parameters
   prng:setSeed( system.cur():nameRaw() )
   local theta = prng:random()*2*math.pi
   local phi = prng:random()*2*math.pi
   local psi = prng:random()*2*math.pi
   local rx, ry = vec2.newP( 6+1*prng:random(), 3+3*prng:random() ):get()
   sz = 1+1*prng:random()
   sb = naev.conf().bg_brightness

   -- Initialize shader
   shader = graphics.newShader( string.format(starfield_frag, rx, ry, theta, phi, psi), love_shaders.vertexcode )
   sstarfield = bgshaders.init( shader, sf, {usetex=true} )

   if not params.nolocalstars then
      add_local_stars()
   end
end

function starfield.render( dt )
   -- Get camera properties
   local x, y = camera.get():get()
   local z = camera.getZoom()
   x = x / 1e6
   y = y / 1e6
   shader:send( "u_camera", x*0.5/sf, -y*0.5/sf, sz, z*0.0008*sf )

   sstarfield:render( dt, {1,1,1,sb} )
end

return starfield

