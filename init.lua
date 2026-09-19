-- this is the first file executed when the application starts
-- we have to load the first modules form here

-- updater
Services = {
    --updater = "http://localhost/api/updater.php", --./updater
    --status = "http://localhost/login.php", --./client_entergame | ./client_topmenu
    --websites = "http://localhost/?subtopic=accountmanagement", --./client_entergame "Forgot password and/or email"
    --createAccount = "http://localhost/clientcreateaccount.php", --./client_entergame -- createAccount.lua
    --getCoinsUrl = "http://localhost/?subtopic=shop&step=terms", --./game_market
}

-- Servidores padrao do nosso projeto (openclaw, 100.122.232.250). Com mais
-- de uma entrada aqui o botao "server list" da tela de login fica visivel
-- (EnterGame.setUniqueServer, chamado so quando ha exatamente 1 entrada,
-- some com ele) -- clique nele pra trocar entre os dois. Cada um so entra
-- na lista salva (client_serverlist/serverlist.lua) na PRIMEIRA vez que o
-- cliente abre com um `.otclient/settings` novo; depois disso e o usuario
-- quem edita/remove pela propria tela.
-- Vetusia (2026-09-19): UM unico servidor de proposito. Com Servers_init
-- tendo so uma entrada, entergame.lua chama EnterGame.setUniqueServer()
-- sozinho (ver "if Servers_init then if table.size(Servers_init) == 1
-- then ..."), que esconde os campos de host/porta -- ninguem consegue
-- digitar outro endereco, o cliente so fala com o Vetusia.
Servers_init = {
    -- IMPORTANTE: a CHAVE desta tabela e' o host de verdade usado pra
    -- conectar (entergame.lua le com next(Servers_init) e manda direto
    -- pro campo de host) -- nao e' so' um rotulo. IP publico da Oracle
    -- (VNIC, ephemeral) reservado pro jogo; ver docs/decisoes-e-analises.md.
    ["82.70.89.193"] = {
        ["port"] = 7272,       -- login server (qm/loginserver.py); a porta do mundo (7273/7274/7275) vem na resposta
        ["protocol"] = 772,
        ["httpLogin"] = false
    },
}

g_app.setName("Vetusia");
g_app.setCompactName("vetusia");
g_app.setOrganizationName("vetusia");

g_app.hasUpdater = function()
    return (Services.updater and Services.updater ~= "" and g_modules.getModule("updater"))
end

-- setup logger
g_logger.setLogFile(g_resources.getWorkDir() .. g_app.getCompactName() .. '.log')
g_logger.info(os.date('== application started at %b %d %Y %X'))
g_logger.info("== operating system: " .. g_platform.getOSName())

-- print first terminal message
g_logger.info(g_app.getName() .. ' ' .. g_app.getVersion() .. ' rev ' .. g_app.getBuildRevision() .. ' (' ..
    g_app.getBuildCommit() .. ') built on ' .. g_app.getBuildDate() .. ' for arch ' ..
    g_app.getBuildArch())

-- setup lua debugger
if os.getenv("LOCAL_LUA_DEBUGGER_VSCODE") == "1" then
    require("lldebugger").start()
    g_logger.debug("Started LUA debugger.")
else
    g_logger.debug("LUA debugger not started (not launched with VSCode local-lua).")
end

-- add data directory to the search path
if not g_resources.addSearchPath(g_resources.getWorkDir() .. 'data', true) then
    g_logger.fatal('Unable to add data directory to the search path.')
end

-- add modules directory to the search path
if not g_resources.addSearchPath(g_resources.getWorkDir() .. 'modules', true) then
    g_logger.fatal('Unable to add modules directory to the search path.')
end

g_html.addGlobalStyle('/data/styles/html.css')
g_html.addGlobalStyle('/data/styles/custom.css')

-- try to add mods path too
g_resources.addSearchPath(g_resources.getWorkDir() .. 'mods', true)

-- setup directory for saving configurations
g_resources.setupUserWriteDir(('%s/'):format(g_app.getCompactName()))

-- search all packages
g_resources.searchAndAddPackages('/', '.otpkg', true)

-- load settings
g_configs.loadSettings('/config.otml')

g_modules.discoverModules()

-- libraries modules 0-99
g_modules.autoLoadModules(99)
g_modules.ensureModuleLoaded('corelib')
g_modules.ensureModuleLoaded('gamelib')
g_modules.ensureModuleLoaded('modulelib')
g_modules.ensureModuleLoaded("startup")

g_modules.autoLoadModules(999)
g_modules.ensureModuleLoaded('game_shaders') -- pre load

local function loadModules()
    -- client modules 100-499
    g_modules.autoLoadModules(499)
    g_modules.ensureModuleLoaded('client')

    -- game modules 500-999
    g_modules.autoLoadModules(999)
    g_modules.ensureModuleLoaded('game_interface')

    -- mods 1000-9999
    g_modules.autoLoadModules(9999)
    g_modules.ensureModuleLoaded('client_mods')

    local script = '/' .. g_app.getCompactName() .. 'rc.lua'

    if g_resources.fileExists(script) then
        dofile(script)
    end

    -- uncomment the line below so that modules are reloaded when modified. (Note: Use only mod dev)
    -- g_modules.enableAutoReload()
end

-- run updater, must use data.zip
if g_app.hasUpdater() then
    g_modules.ensureModuleLoaded("updater")
    return Updater.init(loadModules)
end

loadModules()
