// One data file against its type (data_convert.hpp): read the TOML subset, write the values, wrap and check
// them.

#include "data_convert.hpp"

#include "data_encode.hpp"
#include "data_writer.hpp"

namespace nnh::assetc {

bool convert_data(const Schema& schema, const detail::TypeInfo& type, std::string_view text,
                  ConvertedData& out, Diagnostics& problems) {
    toml::Document doc;
    if (!doc.parse(text)) {
        problems.add(doc.problem().line, doc.problem().message);
        return false;
    }
    Encoder e{schema, problems, doc, {}, {}};
    encode_struct(e, type, doc.root(), "", "", 1, 0);
    if (!problems.empty()) return false;
    std::string problem;
    if (!write_data(schema, type, e.body, out.bytes, problem)) {
        problems.add(1, problem);
        return false;
    }
    out.references = std::move(e.references);
    return true;
}

} // namespace nnh::assetc
