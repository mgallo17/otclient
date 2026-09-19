local iconTopMenu = nil
-- @ Minimap
local minimapWidget = nil -- bot fix
local otmm = true
local oldPos = nil
local fullscreenWidget
local virtualFloor = 7
local minimapImported = false
local currentDayTime = {
    h = 12,
    m = 0
}

local function refreshVirtualFloors()
    mapController.ui.layersPanel.layersMark:setMarginTop(((virtualFloor + 1) * 4) - 3)
    mapController.ui.layersPanel.automapLayers:setImageClip((virtualFloor * 14) .. ' 0 14 67')
end

local function onPositionChange()
    local player = g_game.getLocalPlayer()
    if not player then
        return
    end

    local pos = player:getPosition()
    if not pos then
        return
    end

    local minimapWidget = mapController.ui.minimapBorder.minimap
    if not (minimapWidget) or minimapWidget:isDragging() then
        return
    end

    if not minimapWidget.fullMapView then
        minimapWidget:setCameraPosition(pos)
    end

    minimapWidget:setCrossPosition(pos)
    virtualFloor = pos.z
    refreshVirtualFloors()
end

mapController = Controller:new()
mapController:setUI('minimap', modules.game_interface.getMainRightPanel())

function onChangeWorldTime(hour, minute)
--[[ 

check 
tfs c++ (old) : void ProtocolGame::sendWorldTime()
tfs lua (new) : function Player.sendWorldTime(self, time)
Canary: void ProtocolGame::sendTibiaTime(int32_t time)
 ]]

    currentDayTime = {
        h = hour % 24,
        m = minute
    }

    mapController:scheduleEvent(function()
        local nextH = currentDayTime.h
        local nextM = currentDayTime.m + 12
        if nextM >= 60 then
            nextH = nextH + 1
            nextM = nextM - 60
        end

        onChangeWorldTime(nextH, nextM)
    end, 30000, 'dayTime')

    local position = math.floor((124 / (24 * 60)) * ((hour * 60) + minute))
    local mainWidth = 31
    local secondaryWidth = 0

    if (position + 31) >= 124 then
        secondaryWidth = ((position + 31) - 124) + 1
        mainWidth = 31 - secondaryWidth
    end

    mapController.ui.rosePanel.ambients.main:setWidth(mainWidth)
    mapController.ui.rosePanel.ambients.secondary:setWidth(secondaryWidth)

    if secondaryWidth == 0 then
        mapController.ui.rosePanel.ambients.secondary:hide()
    else
        mapController.ui.rosePanel.ambients.secondary:setImageClip('0 0 ' .. secondaryWidth .. ' 31')
        mapController.ui.rosePanel.ambients.secondary:show()
    end

    if mainWidth == 0 then
        mapController.ui.rosePanel.ambients.main:hide()
    else
        mapController.ui.rosePanel.ambients.main:setImageClip(position .. ' 0 ' .. mainWidth .. ' 31')
        mapController.ui.rosePanel.ambients.main:show()
    end
end

function mapController:onInit()
    self.ui.minimapBorder.minimap:getChildById('floorUpButton'):hide()
    self.ui.minimapBorder.minimap:getChildById('floorDownButton'):hide()
    self.ui.minimapBorder.minimap:getChildById('zoomInButton'):hide()
    self.ui.minimapBorder.minimap:getChildById('zoomOutButton'):hide()
    self.ui.minimapBorder.minimap:getChildById('resetButton'):hide()
end

function mapController:onGameStart()
    mapController:registerEvents(g_game, {
        onChangeWorldTime = onChangeWorldTime
    })

    mapController:registerEvents(LocalPlayer, {
        onPositionChange = onPositionChange
    }):execute()

    -- Load Map
    g_minimap.clean()

    local minimapFile = '/minimap'
    local loadFnc = nil

    if otmm then
        minimapFile = minimapFile .. '.otmm'
        loadFnc = g_minimap.loadOtmm
    else
        minimapFile = minimapFile .. '_' .. g_game.getClientVersion() .. '.otcm'
        loadFnc = g_map.loadOtcm
    end

    -- Vetusia (2026-09-19): semeia o minimapa pre-explorado (empacotado
    -- como /default_minimap.otmm, fora do nome que o jogo procura) na
    -- primeira execucao. Sem isso, se o arquivo padrao morasse direto em
    -- /minimap.otmm ele ficaria montado com PRIORIDADE MAIOR que o
    -- diretorio de escrita do usuario (addSearchPath do workdir e' com
    -- pushFront=true, o do write dir nao e') -- toda vez que o jogador
    -- explorasse e saisse, o save ia pro write dir normalmente, mas a
    -- PROXIMA leitura sempre achava a copia estatica primeiro e ignorava
    -- o progresso salvo, dando a impressao de "o mapa sempre reseta".
    if otmm and not g_resources.fileExists(minimapFile) then
        local defaultMinimap = '/default_minimap.otmm'
        if g_resources.fileExists(defaultMinimap) then
            local ok, err = pcall(function()
                local data = g_resources.readFileContents(defaultMinimap)
                g_resources.writeFileContents(minimapFile, data)
                return #data
            end)
            if ok then
                g_logger.info('Seeded ' .. minimapFile .. ' from ' .. defaultMinimap)
            else
                g_logger.warning('Failed to seed minimap: ' .. tostring(err))
            end
        else
            g_logger.info('No ' .. defaultMinimap .. ' bundled, skipping seed')
        end
    end

    if g_resources.fileExists(minimapFile) then
        local ok, err = pcall(loadFnc, minimapFile)
        if not ok then
            g_logger.warning('Failed to load minimap: ' .. tostring(err))
        else
            g_logger.info('Loaded minimap from ' .. minimapFile)
        end
    end

    -- Vetusia (2026-09-19): importa os PNGs do tibiamaps.io (data/
    -- minimap_import/, cobrem o mapa inteiro) UMA vez por instalacao,
    -- SEMPRE -- nao so' quando nao existe /minimap.otmm. Foi exatamente
    -- assim que o mapa completo apareceu no Mac (commit ab53cf9, "import-
    -- once logic"). A importacao e' aditiva: loadImage so' pinta tile que
    -- ainda nao tem MinimapTileWasSeen, entao o que ja veio do .otmm (seu
    -- ou o default semeado acima) fica intacto e so' os buracos sao
    -- preenchidos. Marcador em arquivo (nao so' variavel de sessao) pra
    -- nao repetir os ~2000 PNGs a cada abertura.
    local importMarker = '/minimap_imported.flag'
    if otmm and not minimapImported and not g_resources.fileExists(importMarker) then
        local importDir = '/data/minimap_import'
        if g_resources.directoryExists(importDir) then
            local files = g_resources.listDirectoryFiles(importDir)
            local count = 0
            for _, file in ipairs(files) do
                local x, y, z = file:match('Minimap_Color_(%d+)_(%d+)_(%d+)%.png')
                if x then
                    local pos = { x = tonumber(x), y = tonumber(y), z = tonumber(z) }
                    g_minimap.loadImage(importDir .. '/' .. file, pos, 1.0)
                    count = count + 1
                end
            end
            g_logger.info('Imported ' .. count .. ' minimap tiles from tibiamaps.io')
            if count > 0 then
                g_minimap.saveOtmm(minimapFile)
                g_logger.info('Saved minimap as ' .. minimapFile)
            end
            pcall(function() g_resources.writeFileContents(importMarker, tostring(count)) end)
        end
        minimapImported = true
    end

    self.ui.minimapBorder.minimap:load()
end

function mapController:onGameEnd()
    -- Save Map
    if otmm then
        g_minimap.saveOtmm('/minimap.otmm')
    else
        g_map.saveOtcm('/minimap_' .. g_game.getClientVersion() .. '.otcm')
    end

    self.ui.minimapBorder.minimap:save()
end

function mapController:onTerminate()
    if iconTopMenu then
        iconTopMenu:destroy()
        iconTopMenu = nil
    end
end

function zoomIn()
    mapController.ui.minimapBorder.minimap:zoomIn()
end

function zoomOut()
    mapController.ui.minimapBorder.minimap:zoomOut()
end

function openCyclopediaMap()
    if g_game.getClientVersion() >= 1310 then
        modules.game_cyclopedia.toggle('map')
    else
        return fullscreen()
    end
end

function fullscreen()
    local minimapWidget = mapController.ui.minimapBorder.minimap
    if not minimapWidget then
        minimapWidget = fullscreenWidget
    end
    local zoom;

    if not minimapWidget then
        return
    end

    if minimapWidget.fullMapView then
        fullscreenWidget = nil
        minimapWidget:setParent(mapController.ui.minimapBorder)
        minimapWidget:fill('parent')
        mapController.ui:show()
        zoom = minimapWidget.zoomMinimap
        g_keyboard.unbindKeyDown('Escape')
        minimapWidget.fullMapView = false
    else
        fullscreenWidget = minimapWidget
        mapController.ui:hide(true)
        minimapWidget:setParent(modules.game_interface.getRootPanel())
        minimapWidget:fill('parent')
        zoom = minimapWidget.zoomFullmap
        g_keyboard.bindKeyDown('Escape', fullscreen)
        minimapWidget.fullMapView = true
    end

    local pos = oldPos or minimapWidget:getCameraPosition()
    oldPos = minimapWidget:getCameraPosition()
    minimapWidget:setZoom(zoom)
    minimapWidget:setCameraPosition(pos)
end

function upLayer()
    if virtualFloor == 0 then
        return
    end

    mapController.ui.minimapBorder.minimap:floorUp(1)
    virtualFloor = virtualFloor - 1
    refreshVirtualFloors()
end

function downLayer()
    if virtualFloor == 15 then
        return
    end

    mapController.ui.minimapBorder.minimap:floorDown(1)
    virtualFloor = virtualFloor + 1
    refreshVirtualFloors()
end

function onClickRoseButton(dir)
    if dir == 'north' then
        mapController.ui.minimapBorder.minimap:move(0, 1)
    elseif dir == 'north-east' then
        mapController.ui.minimapBorder.minimap:move(-1, 1)
    elseif dir == 'east' then
        mapController.ui.minimapBorder.minimap:move(-1, 0)
    elseif dir == 'south-east' then
        mapController.ui.minimapBorder.minimap:move(-1, -1)
    elseif dir == 'south' then
        mapController.ui.minimapBorder.minimap:move(0, -1)
    elseif dir == 'south-west' then
        mapController.ui.minimapBorder.minimap:move(1, -1)
    elseif dir == 'west' then
        mapController.ui.minimapBorder.minimap:move(1, 0)
    elseif dir == 'north-west' then
        mapController.ui.minimapBorder.minimap:move(1, 1)
    end
end

function resetMap()
    mapController.ui.minimapBorder.minimap:reset()
    local player = g_game.getLocalPlayer()
    if player then
        virtualFloor = player:getPosition().z
        refreshVirtualFloors()
    end
end

function getMiniMapUi()
    return mapController.ui.minimapBorder.minimap
end

function extendedView(extendedView)
    if extendedView then
        if not iconTopMenu then
            iconTopMenu = modules.client_topmenu.addTopRightToggleButton('miniMap', tr('Show miniMap'),
                '/images/topbuttons/minimap', toggle)
            iconTopMenu:setOn(mapController.ui:isVisible())
            mapController.ui:setBorderColor('black')
            mapController.ui:setBorderWidth(2)
        end
    else
        if iconTopMenu then
            iconTopMenu:destroy()
            iconTopMenu = nil
        end
        mapController.ui:setBorderColor('alpha')
        mapController.ui:setBorderWidth(0)
        local mainRightPanel = modules.game_interface.getMainRightPanel()
        if not mainRightPanel:hasChild(mapController.ui) then
            mainRightPanel:insertChild(1, mapController.ui)
        end
        mapController.ui:show()

    end
    mapController.ui.moveOnlyToMain = not extendedView
end

function toggle()
    if iconTopMenu:isOn() then
        mapController.ui:hide()
        iconTopMenu:setOn(false)
    else
        mapController.ui:show()
        iconTopMenu:setOn(true)
    end
end
