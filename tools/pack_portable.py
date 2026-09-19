#!/usr/bin/env python3
"""Empacota o cliente Vetusia pra Windows/Linux como uma PASTA (dentro de
um .zip ou .tar.gz), nao mais como um "arquivo unico" com o zip colado no
fim do binario.

Historico (2026-09-19): a versao anterior deste script colava um .zip no
fim do binario compilado, confiando em resourcemanager.cpp::
discoverWorkDir montar o proprio executavel como fonte de arquivos (o
PhysFS le arquivos zip pelo CONTEUDO, nao pela extensao, e acha o
diretorio central de tras pra frente, entao "funciona" mesmo com dados
arbitrarios antes do zip). Na pratica, isso se mostrou NAO-DETERMINISTICO
no Mac -- a mesma copia do arquivo, sem nenhuma mudanca, as vezes montava
certo e as vezes falhava ("Unable to add data directory" / "Unable to
load 'corelib' module"), sinal de um bug de memoria no parser de zip do
PhysFS quando os dados nao comecam no offset 0 do arquivo. Um crash
intermitente ao abrir o jogo e' inaceitavel, entao a abordagem foi
trocada (ver tools/pack_macos_app.py pro equivalente do Mac, que usa a
mesma ideia: arquivos de verdade ao lado do binario, sem zip colado).

Agora: o executavel compilado fica numa pasta de verdade, com data/,
modules/, mods/, init.lua e otclientrc.lua como ARQUIVOS REAIS ao lado
dele. discoverWorkDir ja tenta exatamente essa pasta (PHYSFS_getBaseDir(),
o segundo candidato da lista, logo apos tentar montar o proprio binario
como arquivo unico -- essa tentativa so' falha de forma limpa agora, sem
zip pra confundir, e cai pro candidato seguinte, que e' um PHYSFS_mount()
de DIRETORIO REAL, o caminho mais testado e estavel do PhysFS). A pasta e'
compactada num .zip (Windows) ou .tar.gz (Linux) so' pra facilitar a
distribuicao/download -- o usuario extrai e roda o executavel de dentro
da pasta extraida; nao e' mais "um arquivo unico executavel", mas
continua sendo "um pacote so'" na pratica.

Uso:
    python3 tools/pack_portable.py <binario_compilado> <pasta_saida> \
        --data-dir . [--zip | --tar]
"""
import argparse
import os
import shutil
import sys
import tarfile
import zipfile

# So' o que o cliente realmente le em runtime (ver discoverWorkDir e os
# modulos) -- nada de fonte C++, build/, .git, etc.
INCLUDE = ["data", "modules", "mods", "init.lua", "otclientrc.lua", "minimap.otmm", "config.otml"]

# O binario compilado se chama "otclient"/"otclient.exe" (CMakeLists.txt
# ainda usa project(otclient), sem trocar isso o executavel que o
# jogador ve continuaria com o nome errado -- renomeado aqui na hora de
# empacotar, mesma ideia do pack_macos_app.py.
EXECUTABLE_NAME = "vetusia"


def build_folder(binary: str, data_dir: str, out_dir: str):
    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir)

    ext = os.path.splitext(binary)[1]  # .exe no Windows, vazio no Linux
    binary_name = EXECUTABLE_NAME + ext
    dest_binary = os.path.join(out_dir, binary_name)
    shutil.copy2(binary, dest_binary)
    if os.name != "nt":
        st = os.stat(dest_binary)
        os.chmod(dest_binary, st.st_mode | 0o111)

    for name in INCLUDE:
        src = os.path.join(data_dir, name)
        dst = os.path.join(out_dir, name)
        if os.path.isdir(src):
            shutil.copytree(src, dst)
        elif os.path.isfile(src):
            shutil.copy2(src, dst)
        else:
            print(f"aviso: '{name}' nao encontrado em {data_dir}, pulando", file=sys.stderr)


def make_zip(out_dir: str, zip_path: str):
    base_name = os.path.basename(out_dir.rstrip("/"))
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for root, _dirs, files in os.walk(out_dir):
            for f in files:
                full = os.path.join(root, f)
                arc = os.path.join(base_name, os.path.relpath(full, out_dir))
                z.write(full, arc)


def make_tar(out_dir: str, tar_path: str):
    base_name = os.path.basename(out_dir.rstrip("/"))
    with tarfile.open(tar_path, "w:gz") as t:
        t.add(out_dir, arcname=base_name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("output", help="pasta de saida (ex.: dist/vetusia-windows)")
    ap.add_argument("--data-dir", default=".")
    ap.add_argument("--zip", action="store_true", help="tambem gera <output>.zip")
    ap.add_argument("--tar", action="store_true", help="tambem gera <output>.tar.gz")
    a = ap.parse_args()

    build_folder(a.binary, a.data_dir, a.output)

    size_mb = sum(
        os.path.getsize(os.path.join(root, f))
        for root, _dirs, files in os.walk(a.output)
        for f in files
    ) / 1024 / 1024
    print(f"empacotado: {a.output}/ ({size_mb:.1f} MB)")

    if a.zip:
        zip_path = a.output.rstrip("/") + ".zip"
        make_zip(a.output, zip_path)
        print(f"gerado: {zip_path}")

    if a.tar:
        tar_path = a.output.rstrip("/") + ".tar.gz"
        make_tar(a.output, tar_path)
        print(f"gerado: {tar_path}")


if __name__ == "__main__":
    main()
