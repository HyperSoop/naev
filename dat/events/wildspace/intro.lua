--[[
<?xml version='1.0' encoding='utf8'?>
<event name="Welcome to Wild Space">
 <location>enter</location>
 <unique />
 <chance>100</chance>
 <system>Nirtos</system>
</event>
--]]
local vn = require "vn"
local vni = require "vnimage"
local der = require "common.derelict"

local mainspb, mainsys = spob.getS("Hypergate Protera")

local derelict
function create ()
   evt.finish(false)

   -- Create the random derelict
   derelict = pilot.add( "Koala", "Derelict", vec2.newP( system.cur():radius()*0.3, rnd.angle() ), p_("ship", "Derelict"), {ai="dummy", naked=true})
   derelict:disable()
   derelict:intrinsicSet( "ew_hide", 300 ) -- Much more visible
   derelict:intrinsicSet( "nebu_absorb", 100 ) -- Immune to nebula

   -- Don't let the player land untiil the hail, we could do a diff, but it's more complex and likely not worth it
   player.landAllow( false, _("You do not see any place to land here.") )

   -- Set up our hooks
   hook.timer( 25, "msg" )
   hook.enter( "enter" )
   hook.land( "land" )
end

function msg ()
   -- Shouldn't be dead, but just in case
   if not derelict:exists() then
      return
   end
   derelict:setVisplayer()
   derelict:hailPlayer()
   hook.pilot(derelict, "hail", "hail")
end

function hail ()
   vn.clear()
   vn.scene()
   local c = vn.newCharacter( vni.soundonly( _("character","C") ) )
   vn.transition()

   vn.na(_([[You respond to the mysterious hail from a derelict, and strangely enough are able to open a sound-only channel.]]))
   c(_([["You aren't one of them, are you?"]]))
   vn.menu{
      {_("Who are YOU?"), "cont01_you"},
      {_("Them?"), "cont01"},
   }

   vn.label("cont01_you")
   c(_([[You hear a loud cough.
"Not one of them at least."]]))
   vn.label("cont01")
   c(_([["What are you doing out there? You'll get shredded out there! Come land over here."]]))
   vn.na(_([[You hear some coughing as the connection is cut. It looks like they sent you some coordinates though.]]))

   vn.run()

   -- Not a mission so use a system marker
   system.markerAdd( mainspb:pos(), _("Land here") )
   player.landAllow()
end

function land ()
   vn.clear()
   vn.scene()
   vn.transition()

   vn.sfx( der.sfx.board )
   vn.na(_([[You slowly approach the location you were given with your ship avoiding the thick structural debris. After a long time of searching, you eventually find a crusty old docking port among the sprawling wreck that seemingly is in working condition. Weapon drawn and in full EVA gear you prepare to go into the rubbish.]]))
   vn.na(_([[You work your way through the crushed structure]]))

   vn.scene()
   local c = vn.newCharacter( vni.soundonly( _("character","C") ) )
   c(_([[]]))

   vn.run()

   faction.get("Lost"):setKnown(true)
end

function enter ()
   if system.cur() ~= mainsys then
      evt.finish(false)
   end
end
