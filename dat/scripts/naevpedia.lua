local cmark = require "cmark"
local lyaml = require "lyaml"
local lf = require "love.filesystem"
local luatk = require 'luatk'
local md = require "luatk.markdown"
local fmt = require "format"
local utf8 = require "utf8"

local naevpedia = {}

local function strsplit( str, sep )
   sep = sep or "%s"
   local t={}
   for s in utf8.gmatch(str, "([^"..sep.."]+)") do
      table.insert(t, s)
   end
   return t
end

--[[--
Pulls out the metadata header of the naevpedia file.
--]]
local function extractmetadata( entry, s )
   local path = strsplit( entry, "/" )
   local meta = {
      entry = entry,
      category = path[1],
      name = path[#path],
   }
   if utf8.find( s, "---\n", 1, true )==1 then
      local es, ee = utf8.find( s, "---\n", 4, true )
      meta = lyaml.load( utf8.sub( s, 4, es-1 ) )
      s = utf8.sub( s, ee+1 )
   end
   return s, meta
end

-- Load into the cache to avoid having to slurp all the files all the time
local nc = naev.cache()
function naevpedia.load()
   local mds = {}
   local function find_md( dir )
      for k,v in ipairs(lf.getDirectoryItems('naevpedia/'..dir)) do
         local f = v
         if dir ~= "" then
            f = dir.."/"..f
         end
         local i = lf.getInfo( 'naevpedia/'..f )
         if i then
            if i.type == "file" then
               local suffix = utf8.sub(f, -3)
               if suffix=='.md' then
                  local dat = lf.read( 'naevpedia/'..f )
                  local entry = utf8.sub( f, 1, -4 )
                  local _s, meta = extractmetadata( entry, dat )
                  mds[ entry ] = meta
               end
            elseif i.type == "directory" then
               find_md( f )
            end
         end
      end
   end
   find_md( '' )
   nc._naevpedia = mds
end
-- See if we have to load the naevpedia, since we want to cache it for speed
if not nc._naevpedia then
   naevpedia.load()
end

--[[--
Processes the Lua in the markdown file as nanoc does.

<%= print('foo') %> statements get printed in the text, while <% if foo then %> get processed otherwise.
--]]
local function dolua( s )
   -- Do early stopping if no Lua is detected
   local ms, me = utf8.find( s, "<%", 1, true )
   if not ms then
      return true, s
   end

   -- Start up the Lua stuff
   local luastr = [[local out = ""
local pr = _G.print
local pro = function( str )
   out = out..str
end
]]
   local function embed_str( str )
      for i=1,20 do
         local sep = ""
         for j=1,i do
            sep = sep.."="
         end
         if (not utf8.find(str,"["..sep.."[",1,true)) and (not utf8.find(str,"]"..sep.."]",1,true)) then
            luastr = luastr.."out = out..["..sep.."["..str.."]"..sep.."]\n"
            break
         end
      end
   end

   local be = 1
   while ms do
      local bs
      local display = false
      embed_str( utf8.sub( s, be+1, ms-1 ) )
      bs, be = utf8.find( s, "%>", me, true )
      if utf8.sub( s, me+1, me+1 )=="=" then
         me = me+1
         display = true
         luastr = luastr.."_G.print = pro\n"
      else
         luastr = luastr.."_G.print = pr\n"
      end
      local ss = utf8.sub( s, me+1, bs-1 )
      luastr = luastr..ss.."\n"
      if display then
         luastr = luastr.."out = out..'\\n'\n"
      end
      ms, me = utf8.find( s, "<%", me, true )
   end
   embed_str( utf8.sub( s, be+1 ) )
   luastr = luastr.."return out"
   local pr = _G.print
   local c = loadstring(luastr)
   --setfenv( c, { _G={}, print=pr } )
   local success,result_or_err = pcall( c )
   _G.print = pr
   if not success then
      warn( result_or_err )
      return "#r"..result_or_err.."#0"
   end
   return success, result_or_err
end

--[[--
Parse document
--]]
local function loaddoc( filename )
   local meta = {}

   -- Load the file
   local rawdat = lf.read( 'naevpedia/'..filename..'.md' )
   if not rawdat then
      warn(fmt.f(_("File '{filename}' not found!"),{filename=filename}))
      return false, fmt.f("#r".._("404\nfile '{filename}' not found"), {filename=filename}), meta
   end

   -- Extract metadata
   rawdat, meta = extractmetadata( filename, rawdat )

   -- Preprocess Lua
   local success, dat = dolua( rawdat )
   if not success then
      return success, dat, meta
   end

   -- Finally parse the remaining text as markdown
   return success, cmark.parse_string( dat, cmark.OPT_DEFAULT ), meta
end

function naevpedia.open( name )
   name = name or "index"

   local history = {}
   local historyrev = {}
   local current = "index"

   -- Set up the window
   local open_page
   local w, h = naev.gfx.dim()
   local wdw = luatk.newWindow( nil, nil, w, h )
   luatk.newText( wdw, 0, 10, w, 20, _("Naevpedia"), nil, "center" )
   luatk.newButton( wdw, -20, -20, 80, 30, _("Close"), luatk.close )

   local btnback, btnfwd
   local function goback ()
      local n = #history
      table.insert( historyrev, current )
      current = history[n]
      history[n] = nil
      open_page( current )
      if #history <= 0 then
         btnback:disable()
         btnfwd:enable()
      end
   end
   local function gofwd ()
      local n = #historyrev
      table.insert( history, current )
      current = historyrev[n]
      historyrev[n] = nil
      open_page( current )
      if #historyrev <= 0 then
         btnfwd:disable()
         btnback:enable()
      end
   end

   -- Backbutton
   btnfwd = luatk.newButton( wdw, -20-80-20, -20, 80, 30, _("Forward"), gofwd )
   if #historyrev <= 0 then
      btnfwd:disable()
   end
   btnback = luatk.newButton( wdw, -20-(80+20)*2, -20, 80, 30, _("Back"), goback )
   if #history <= 0 then
      btnback:disable()
   end
   local mrk
   function open_page( filename )
      if mrk then
         mrk:destroy()
      end

      -- TODO detect if filename is special (like a ship), and grab data from there, or potentially do that when setting up the cache

      -- Load the document
      local success, doc, _meta = loaddoc( filename )

      -- Create widget
      if not success then
         -- Failed, so just display the error
         mrk = luatk.newText( wdw, 20, 40, w-40, h-110, doc )
      else
         -- Success so we try to load the markdown
         mrk = md.newMarkdown( wdw, doc, 20, 40, w-40, h-110, {
            linkfunc = function ( target )
               local newdoc = target
               if not newdoc then
                  -- do warning
                  luatk.msg( _("404"), fmt.f(_("Unable to find link to '{target}'!"),{target=target}))
                  return
               end
               table.insert( history, current )
               btnback:enable()
               current = target
               mrk:destroy()
               open_page( newdoc )
               -- Clear forward history
               historyrev = {}
               btnfwd:disable()
            end,
         } )

         -- Clean up the document
         cmark.node_free( doc )
      end
   end
   open_page( name )
   wdw:setCancel( function ()
      wdw:destroy()
      return true
   end )
   wdw:setKeypress( function ( key )
      if key=="left" then
         goback()
      elseif key=="right" then
         gofwd()
      end
   end )
   luatk.run()
end

return naevpedia
