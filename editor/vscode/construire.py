#!/usr/bin/env python3
"""Construit ceflo.vsix : une archive zip qui contient l'extension et sa description."""

import fnmatch
import json
import os
import zipfile

ICI = os.path.dirname(os.path.abspath(__file__))

CONTENT_TYPES = """<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension=".json" ContentType="application/json"/>
<Default Extension=".js" ContentType="application/javascript"/>
<Default Extension=".md" ContentType="text/markdown"/>
<Default Extension=".vsixmanifest" ContentType="text/xml"/>
</Types>
"""

MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
<Metadata>
<Identity Language="en-US" Id="{name}" Version="{version}" Publisher="{publisher}"/>
<DisplayName>{displayName}</DisplayName>
<Description xml:space="preserve">{description}</Description>
<Categories>Programming Languages</Categories>
</Metadata>
<Installation>
<InstallationTarget Id="Microsoft.VisualStudio.Code"/>
</Installation>
<Dependencies/>
<Assets>
<Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
</Assets>
</PackageManifest>
"""


def fichiers_ignores():
    with open(os.path.join(ICI, ".vscodeignore"), encoding="utf-8") as fichier:
        return [ligne.strip() for ligne in fichier if ligne.strip()]


def fichiers_a_publier():
    ignores = fichiers_ignores()
    for dossier, _, noms in os.walk(ICI):
        for nom in noms:
            chemin = os.path.relpath(os.path.join(dossier, nom), ICI)
            if not any(fnmatch.fnmatch(chemin, motif) for motif in ignores) and chemin != ".vscodeignore":
                yield chemin


def main():
    with open(os.path.join(ICI, "package.json"), encoding="utf-8") as fichier:
        paquet = json.load(fichier)

    with zipfile.ZipFile(os.path.join(ICI, "ceflo.vsix"), "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", CONTENT_TYPES)
        archive.writestr("extension.vsixmanifest", MANIFEST.format(**paquet))
        for chemin in sorted(fichiers_a_publier()):
            archive.write(os.path.join(ICI, chemin), "extension/" + chemin)


if __name__ == "__main__":
    main()
