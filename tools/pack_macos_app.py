#!/usr/bin/env python3
"""Empacota o cliente Vetusia como um .app do macOS, assinado (ad-hoc).

Por que um .app: no Finder, qualquer executavel Unix "cru" (sem ser um
bundle) e' aberto DENTRO do Terminal.app -- isso e' um comportamento do
LaunchServices, nao tem como evitar so' com flags de compilacao. Um .app
e' uma PASTA que o Finder/LaunchServices mostram como um icone unico e
sabem executar diretamente, sem terminal.

Por que NAO usamos o truque de "zip colado no binario" aqui (ao contrario
de versoes antigas deste script): testado na pratica (2026-09-19) que o
mount desse arquivo-unico-com-zip-anexado pelo PhysFS e'
NAO-DETERMINISTICO -- o mesmo arquivo, sem nenhuma mudanca, as vezes
inicia normalmente e as vezes falha ("Unable to add data directory" /
"Unable to load 'corelib' module"), sinal de um bug de memoria no parser
de zip do PhysFS quando os dados nao comecam no offset 0 do arquivo. Em
vez disso, os arquivos (data/modules/mods/etc.) vao como ARQUIVOS DE
VERDADE dentro do bundle.

Onde exatamente: em Contents/Resources/, a convencao padrao da Apple --
NAO na raiz do bundle como uma versao anterior deste script fazia.
Historico (2026-09-19): colocar os arquivos soltos na raiz do bundle
(fora de Contents/) fazia sentido porque PHYSFS_getBaseDir() no macOS
retorna a raiz do .app, nao Contents/MacOS/ -- mas isso IMPEDE assinar o
bundle (`codesign` recusa com "unsealed contents present in the bundle
root"), e um .app baixado da internet sem assinatura valida vira "app
esta danificado" no Gatekeeper (nao e' so' o aviso de "desenvolvedor nao
identificado", que pelo menos da' pra abrir manualmente -- "danificado"
BLOQUEIA de vez). A correcao real foi no C++: discoverWorkDir() (ver
resourcemanager.cpp) ganhou um candidato extra pra macOS que olha
Contents/Resources/ relativo ao proprio binario, entao os dados podem
ficar no lugar certo (dentro de Contents/) e o bundle inteiro pode ser
assinado normalmente.

Uso:
    python3 tools/pack_macos_app.py <binario_compilado> <saida.app> \
        --data-dir . --icns cmake/icon/vetusia.icns

Assinatura: ad-hoc (`codesign --sign -`), nao um Developer ID pago --
ainda pede confirmacao no Gatekeeper na primeira vez ("nao foi possivel
verificar o desenvolvedor", com um botao "Abrir mesmo assim" em
Ajustes > Privacidade e Seguranca), mas isso e' bem diferente de "app
esta danificado, mova pra lixeira" (que bloqueia sem alternativa). E' o
mesmo caso do OTClient original e da maioria dos clientes de Tibia
privados sem certificado pago.
"""
import argparse
import os
import plistlib
import shutil
import subprocess
import sys

BUNDLE_ID = "com.vetusia.client"
BUNDLE_NAME = "Vetusia"
EXECUTABLE_NAME = "vetusia"
ICON_NAME = "vetusia"  # sem extensao, resolve pra vetusia.icns

# So' o que o cliente realmente le em runtime (ver discoverWorkDir e os
# modulos) -- igual ao INCLUDE de pack_portable.py.
INCLUDE = ["data", "modules", "mods", "init.lua", "otclientrc.lua", "default_minimap.otmm", "config.otml"]


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


def copy_tree_contents(data_dir: str, resources_dir: str):
    for name in INCLUDE:
        src = os.path.join(data_dir, name)
        dst = os.path.join(resources_dir, name)
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

    copy_tree_contents(a.data_dir, resources_dir)

    shutil.copy2(a.icns, os.path.join(resources_dir, ICON_NAME + ".icns"))
    build_info_plist(os.path.join(a.output, "Contents", "Info.plist"))

    # Ad-hoc: tudo dentro de Contents/ agora (nada solto na raiz do
    # bundle), entao o codesign consegue selar o bundle inteiro.
    subprocess.run(["codesign", "--deep", "--force", "--sign", "-", a.output], check=True)

    size_mb = sum(
        os.path.getsize(os.path.join(root, f))
        for root, _dirs, files in os.walk(a.output)
        for f in files
    ) / 1024 / 1024
    print(f"empacotado e assinado (ad-hoc): {a.output} ({size_mb:.1f} MB)")
    print("aviso: copia baixada da internet ainda pede 'Abrir mesmo assim' no Gatekeeper (sem Developer ID pago nao da' pra evitar isso), mas nao mostra mais 'app esta danificado'")


if __name__ == "__main__":
    main()
