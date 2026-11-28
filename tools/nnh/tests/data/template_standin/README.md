# Template stand-in

A tiny template for `test_template_render.cpp` (spec 033 T2.4): `tools/nnh/CMakeLists.txt` embeds four of these files
with `tools/nnh/embed_template.cmake`, exactly as it embeds `tools/nnh/template/`. `greeting.txt.in` is kept with CRLF
line endings (`-text` in `.gitattributes`) to check the LF normalisation; `art/blob.png` is not an image: its bytes
(NUL, CR LF pairs, 0xFF) check that binaries are embedded byte for byte. This README is not embedded.
