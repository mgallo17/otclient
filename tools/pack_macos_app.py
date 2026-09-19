#!/usr/bin/env python3
"""Empacota o cliente Vetusia como um .app do macOS.

Por que um .app: no Finder, qualquer executavel Unix "cru" (sem ser um
bundle) e' aberto DENTRO do Terminal.app -- isso e' um comportamento do
LaunchServices, nao tem como evitar so' com flags de compilacao. Um .app
e' uma PASTA que o Finder/LaunchServices mostram como um icone unico e
sabem executar diretamente, sem terminal -- e' o formato nativo pra "um
arquivo so" no Mac (o usuario ve/arrasta/copia UM icone, exatamente como
qualquer outro app).

Por que NAO usamos o truque de "zip colado no binario" aqui (ao contrario
de pack_portable.py, usado no Windows/Linux): testado na pratica
(2026-09-19) que o mount desse arquivo-unico-com-zip-anexado pelo PhysFS
e' NAO-DETERMINISTICO -- o mesmo arquivo, sem nenhuma mudanca, as vezes
inicia normalmente e as vezes falha com "Unable to add data directory"
ou "Unable to load 'corelib' module". Isso indica um bug de memoria no
parser de zip do PhysFS quando os dados nao comecam no offset 0 do
arquivo -- inaceitavel pra um client que jogadores vao abrir. Em vez
disso, aqui a gente copia data/modules/mods/init.lua/otclientrc.lua como
ARQUIVOS DE VERDADE dentro do bundle, ao lado do binario (Contents/
MacOS/). resourcemanager.cpp::discoverWorkDir ja tenta exatamente esse
caminho (PHYSFS_getBaseDir(), a segunda opcao da lista, logo depois de
tentar montar o proprio binario como arquivo unico) -- so' que dessa vez
e' um PHYSFS_mount() de um DIRETORIO REAL, o caminho mais testado e
estavel do PhysFS, sem risco nenhum de parsing de zip.

Uso:
    python3 tools/pack_macos_app.py <binario_compilado> <saida.app> \
        --data-dir . --icns cmake/icon/vetusia.icns

NAO assinamos o bundle: o codesign exige que Contents/MacOS/ contenha SO'
o executavel (convencao Apple) -- com data/modules/mods soltos ali do
lado (pra reaproveitar o candidato pronto do discoverWorkDir), qualquer
arquivo dentro vira um "subcomponente" que o codesign recusa assinar sem
assinatura propria. Sem problema: o binario interno nunca e' modificado
depois de compilado, sua assinatura ad-hoc original do build continua
100% valida, e um .app sem assinatura de bundle roda normalmente numa
copia local (nao baixada da internet).

Uma copia BAIXADA da internet carrega o atributo de
quarentena do macOS e o Gatekeeper pede confirmacao na primeira vez
("nao foi possivel verificar o desenvolvedor") -- sem um Developer ID
pago (assinatura + notarizacao reais) nao da' pra evitar esse aviso; e'
o mesmo caso do OTClient original e da maioria dos clientes de Tibia
privados. Solucao pratica pro usuario final: botao direito -> Abrir (pula
a checagem na primeira vez) ou `xattr -cr Vetusia.app` depois de baixar.

Nota sobre ONDE os arquivos ficam dentro do bundle: PHYSFS_getBaseDir()
no macOS retorna a RAIZ do .app (ex.: "Vetusia.app/"), nao
"Contents/MacOS/" (confirmado na pratica com um mini-teste de PHYSFS) --
diferente de Linux/Windows, onde base dir e' o diretorio do executavel.
Por isso data/modules/mods/init.lua/otclientrc.lua vao na RAIZ do bundle
(irmãos de "Contents/", nao dentro dele) -- e' onde o segundo candidato
de discoverWorkDir (getBaseDir()) de fato vai procurar. Nao e' a
convencao usual de bundle (que poe recursos em Contents/Resources/), mas
funciona sem precisar mexer no C++, e o Finder/LaunchServices tratam o
.app inteiro como um icone unico de qualquer forma.
"""
import argparse
import os
import plistlib
import shutil
import sys

BUNDLE_ID = "com.vetusia.client"
BUNDLE_NAME = "Vetusia"
EXECUTABLE_NAME = "vetusia"
ICON_NAME = "vetusia"  # sem extensao, resolve pra vetusia.icns

# So' o que o cliente realmente le em runtime (ver discoverWorkDir e os
# modulos) -- igual ao INCLUDE de pack_portable.py.
INCLUDE = ["data", "modules", "mods", "init.lua", "otclientrc.lua", "minimap.otmm"]


def build_info_plist(path: str):
    data = {
        "CFBundleExecutable": EXECUTABLE_NAME,
        "CFBundleIconFile": ICON_NAME,
        "CFBundleIdentifier": BUNDLE_ID,
        "CFBundleName": BUNDLE_NAME,
        "CFBundleDisplayName": BUNDLE_NAME,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "1.0.0",
        "CFBundleVersion": "1.0.0",
        "CFBundleInfoDictionaryVersion": "6.0",
        "LSMinimumSystemVersion": "11.0",
        "NSHighResolutionCapable": True,
        "NSHumanReadableCopyright": "Vetusia",
    }
    with open(path, "wb") as f:
        plistlib.dump(data, f)


def copy_tree_contents(data_dir: str, macos_dir: str):
    for name in INCLUDE:
        src = os.path.join(data_dir, name)
        dst = os.path.join(macos_dir, name)
        if os.path.isdir(src):
            shutil.copytree(src, dst)
        elif os.path.isfile(src):
            shutil.copy2(src, dst)
        else:
            print(f"aviso: '{name}' nao encontrado em {data_dir}, pulando", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("output", help="caminho do .app de saida (ex.: Vetusia.app)")
    ap.add_argument("--data-dir", default=".")
    ap.add_argument("--icns", required=True)
    a = ap.parse_args()

    if not a.output.endswith(".app"):
        a.output += ".app"

    if os.path.exists(a.output):
        shutil.rmtree(a.output)

    macos_dir = os.path.join(a.output, "Contents", "MacOS")
    resources_dir = os.path.join(a.output, "Contents", "Resources")
    os.makedirs(macos_dir)
    os.makedirs(resources_dir)

    inner_binary = os.path.join(macos_dir, EXECUTABLE_NAME)
    shutil.copy2(a.binary, inner_binary)
    os.chmod(inner_binary, 0o755)

    # ver nota no docstring do modulo: PHYSFS_getBaseDir() no macOS da' a
    # RAIZ do .app, entao os arquivos vao ali, nao em Contents/MacOS/.
    copy_tree_contents(a.data_dir, a.output)

    shutil.copy2(a.icns, os.path.join(resources_dir, ICON_NAME + ".icns"))
    build_info_plist(os.path.join(a.output, "Contents", "Info.plist"))

    size_mb = sum(
        os.path.getsize(os.path.join(root, f))
        for root, _dirs, files in os.walk(a.output)
        for f in files
    ) / 1024 / 1024
    print(f"empacotado: {a.output} ({size_mb:.1f} MB)")
    print("aviso: copia baixada da internet vai pedir 'Abrir mesmo assim' no Gatekeeper (sem Developer ID pago nao da' pra evitar)")


if __name__ == "__main__":
    main()
