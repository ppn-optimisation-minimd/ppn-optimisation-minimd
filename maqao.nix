{ lib
, stdenv
, fetchurl
}:

stdenv.mkDerivation rec {
  pname = "maqao";
  version = "2026.1.0";

  src = fetchurl {
    url = "http://www.maqao.org/maqao_archive/maqao.x86_64.${version}.tar.xz";
    hash = "sha256-tESTLDiGF+zFEd2LNkxXHroCFcBe6w4Su7xY7gWcAFY=";
  };

  dontConfigure = true;
  dontBuild = true;

  dontStrip = true;
  dontPatchELF = true;

  installPhase = ''
    runHook preInstall
    install -Dm755 bin/maqao $out/bin/maqao
    mkdir -p $out/share/man
    cp -r man/. $out/share/man/
    install -Dm644 LICENSE $out/share/doc/maqao/LICENSE
    runHook postInstall
  '';

  meta = {
    description = "Binary-level performance analysis framework (LProf, CQA, ONE View)";
    homepage = "https://maqao.org";
    license = lib.licenses.lgpl21Plus;
    platforms = [ "x86_64-linux" ];
    sourceProvenance = [ lib.sourceTypes.binaryNativeCode ];
    mainProgram = "maqao";
  };
}
