#!/usr/bin/env python3
"""glTF 2.0 fixtures for nnh-assetc's mesh converter (spec 026 T3.1, AC-4). Original data, written by this script
(no third-party models). Run from anywhere: python -I tests/data/gltf/make_fixtures.py — it rewrites the files beside
it. Each bad_<rule>.gltf has a bad_<rule>.txt holding the part of the refusal message the tests expect."""

import json
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def write(name, data):
    mode = "w" if isinstance(data, str) else "wb"
    with open(os.path.join(HERE, name), mode, newline="\n" if mode == "w" else None) as f:
        f.write(data)


def png(width, height, pixel):
    """An RGBA PNG; pixel(x, y) -> (r, g, b, a)."""
    raw = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


class Bin:
    """One buffer and its views/accessors."""

    def __init__(self):
        self.data = b""
        self.views = []
        self.accessors = []

    def add(self, fmt, values, comps, ctype, kind, normalized=False, minmax=False, target=None):
        while len(self.data) % 4:
            self.data += b"\x00"
        offset = len(self.data)
        self.data += struct.pack("<" + fmt * len(values), *values)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(self.data) - offset}
        if target:
            view["target"] = target
        self.views.append(view)
        acc = {"bufferView": len(self.views) - 1, "componentType": ctype, "count": len(values) // comps, "type": kind}
        if normalized:
            acc["normalized"] = True
        if minmax:
            acc["min"] = [min(values[i::comps]) for i in range(comps)]
            acc["max"] = [max(values[i::comps]) for i in range(comps)]
        self.accessors.append(acc)
        return len(self.accessors) - 1


def cube_arrays():
    faces = [((1, 0, 0), (0, 0, -1), (0, 1, 0)), ((-1, 0, 0), (0, 0, 1), (0, 1, 0)), ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
             ((0, -1, 0), (1, 0, 0), (0, 0, 1)), ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (-1, 0, 0), (0, 1, 0))]
    pos, nrm, uv, idx = [], [], [], []
    for n, u, v in faces:
        base = len(pos) // 3
        for su, sv, tu, tv in ((-1, -1, 0, 1), (1, -1, 1, 1), (1, 1, 1, 0), (-1, 1, 0, 0)):
            pos += [0.5 * (n[i] + su * u[i] + sv * v[i]) for i in range(3)]
            nrm += list(n)
            uv += [tu, tv]
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return pos, nrm, uv, idx


def document(b, meshes, nodes, materials=None, extra=None, buffer_uri="x.bin"):
    doc = {"asset": {"version": "2.0", "generator": "NNHGE make_fixtures.py"}, "scene": 0,
           "scenes": [{"nodes": [i for i, n in enumerate(nodes) if not n.pop("_child", False)]}], "nodes": nodes,
           "meshes": meshes, "buffers": [{"uri": buffer_uri, "byteLength": len(b.data)}], "bufferViews": b.views,
           "accessors": b.accessors}
    if materials:
        doc["materials"] = materials
    if extra:
        doc.update(extra)
    return doc


def save(name, doc, b):
    write(name + ".gltf", json.dumps(doc, indent=1) + "\n")
    write(name + ".bin", b.data)


def cube_textured():
    pos, nrm, uv, idx = cube_arrays()
    b = Bin()
    p = b.add("f", pos, 3, 5126, "VEC3", minmax=True)
    n = b.add("f", nrm, 3, 5126, "VEC3")
    t = b.add("f", uv, 2, 5126, "VEC2")
    i = b.add("H", idx, 1, 5123, "SCALAR")
    prim = {"attributes": {"POSITION": p, "NORMAL": n, "TEXCOORD_0": t}, "indices": i, "material": 0}
    extra = {"textures": [{"source": 0, "sampler": 0}], "images": [{"uri": "checker.png"}],
             "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 10497}]}
    mat = [{"name": "checker", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                        "baseColorFactor": [1, 0.5, 0.25, 1]}}]
    save("cube_textured", document(b, [{"primitives": [prim]}], [{"mesh": 0}], mat, extra, "cube_textured.bin"), b)


def triangle_bin(colors=False, normals=True):
    b = Bin()
    p = b.add("f", [0, 0, 0, 1, 0, 0, 0, 1, 0], 3, 5126, "VEC3", minmax=True)
    attrs = {"POSITION": p}
    if normals:
        attrs["NORMAL"] = b.add("f", [0, 0, 1] * 3, 3, 5126, "VEC3")
    if colors:
        attrs["COLOR_0"] = b.add("B", [255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 128], 4, 5121, "VEC4", normalized=True)
    return b, attrs


def multi_node():
    b, attrs = triangle_bin()
    nodes = [{"mesh": 0, "translation": [2, 0, 0], "children": [1]},
             {"mesh": 0, "rotation": [0, 0.70710678, 0, 0.70710678], "scale": [1, 2, 1], "_child": True},
             {"mesh": 0, "scale": [-1, 1, 1], "translation": [0, 0, -3]}]
    save("multi_node", document(b, [{"primitives": [{"attributes": attrs}]}], nodes, buffer_uri="multi_node.bin"), b)


def vertex_colors():
    b, attrs = triangle_bin(colors=True)
    save("vertex_colors", document(b, [{"primitives": [{"attributes": attrs}]}], [{"mesh": 0}],
                                   buffer_uri="vertex_colors.bin"), b)


def no_normals():
    b = Bin()
    p = b.add("f", [-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1], 3, 5126, "VEC3", minmax=True)
    i = b.add("H", [0, 2, 1, 0, 3, 2], 1, 5123, "SCALAR")
    save("no_normals", document(b, [{"primitives": [{"attributes": {"POSITION": p}, "indices": i}]}], [{"mesh": 0}],
                                buffer_uri="no_normals.bin"), b)


def cutout_blend():
    b, attrs = triangle_bin()
    mats = [{"alphaMode": "MASK", "alphaCutoff": 0.25, "doubleSided": True},
            {"alphaMode": "BLEND", "extensions": {"KHR_materials_unlit": {}},
             "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.4, 1.0, 0.5]}}]
    prims = [{"attributes": attrs, "material": 0}, {"attributes": attrs, "material": 1},
             {"attributes": attrs, "material": 0}]
    extra = {"extensionsUsed": ["KHR_materials_unlit"]}
    save("cutout_blend", document(b, [{"primitives": prims}], [{"mesh": 0}], mats, extra, "cutout_blend.bin"), b)


def cube_glb():
    pos, nrm, _, idx = cube_arrays()
    b = Bin()
    p = b.add("f", pos, 3, 5126, "VEC3", minmax=True)
    n = b.add("f", nrm, 3, 5126, "VEC3")
    i = b.add("H", idx, 1, 5123, "SCALAR")
    doc = document(b, [{"primitives": [{"attributes": {"POSITION": p, "NORMAL": n}, "indices": i}]}], [{"mesh": 0}])
    del doc["buffers"][0]["uri"]
    text = json.dumps(doc).encode()
    text += b" " * (-len(text) % 4)
    data = b.data + b"\x00" * (-len(b.data) % 4)
    body = struct.pack("<II", len(text), 0x4E4F534A) + text + struct.pack("<II", len(data), 0x004E4942) + data
    write("cube.glb", struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


def bad(name, expected, mutate):
    b, attrs = triangle_bin()
    doc = document(b, [{"primitives": [{"attributes": attrs}]}], [{"mesh": 0}], buffer_uri="bad_" + name + ".bin")
    mutate(doc, b)
    save("bad_" + name, doc, b)
    write("bad_" + name + ".txt", expected + "\n")


def bad_cases():
    def prim(doc):
        return doc["meshes"][0]["primitives"][0]
    bad("mode", "only triangle primitives (mode 4) are supported (at $.meshes[0].primitives[0])",
        lambda d, b: prim(d).update({"mode": 1}))
    bad("sparse", "sparse accessors aren't supported",
        lambda d, b: d["accessors"][0].update({"sparse": {"count": 1}}))
    bad("required_extension", "required extension KHR_draco_mesh_compression isn't supported",
        lambda d, b: d.update({"extensionsRequired": ["KHR_draco_mesh_compression"],
                               "extensionsUsed": ["KHR_draco_mesh_compression"]}))
    bad("embedded_image", "images inside the model aren't supported",
        lambda d, b: (d.update({"materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
                                "textures": [{"source": 0}], "images": [{"bufferView": 0, "mimeType": "image/png"}]}),
                      prim(d).update({"material": 0})))
    bad("data_uri", "embedded data (a data: URI) isn't supported",
        lambda d, b: d["buffers"][0].update({"uri": "data:application/octet-stream;base64,AAAA"}))
    bad("buffer_name", "buffer 'other.bin' must be named bad_buffer_name.bin",
        lambda d, b: d["buffers"][0].update({"uri": "other.bin"}))
    bad("index_range", "an index is out of range",
        lambda d, b: (prim(d).update({"indices": b.add("H", [0, 1, 7], 1, 5123, "SCALAR")}),
                      d["buffers"][0].update({"byteLength": len(b.data)})))
    bad("accessor_range", "the accessor reads past its bufferView or buffer",
        lambda d, b: d["accessors"][0].update({"count": 4}))
    bad("texture_size", "texture sides must be powers of two from 8 to 1024",
        lambda d, b: (d.update({"materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
                                "textures": [{"source": 0}], "images": [{"uri": "odd.png"}]}),
                      prim(d).update({"material": 0})))
    bad("version", "not glTF 2.0 (asset.version is '1.0')", lambda d, b: d["asset"].update({"version": "1.0"}))
    bad("no_scene", "the model has no scene", lambda d, b: (d.pop("scenes"), d.pop("scene")))
    write("bad_json.gltf", '{"asset": {"version": "2.0"},\n "nodes": [}\n')
    write("bad_json.txt", "line 2\n")


def main():
    write("checker.png", png(8, 8, lambda x, y: (255, 255, 255, 255) if (x + y) % 2 == 0 else (32, 64, 200, 255)))
    write("odd.png", png(12, 12, lambda x, y: (255, 0, 0, 255)))
    cube_textured()
    multi_node()
    vertex_colors()
    no_normals()
    cutout_blend()
    cube_glb()
    bad_cases()


if __name__ == "__main__":
    main()
