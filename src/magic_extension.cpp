#define DUCKDB_EXTENSION_MAIN

#include <magic.h>

#define STATIC_MAGIC_FILE

#ifdef STATIC_MAGIC_FILE
#include "magic_mgc.hpp"
#endif
#include "magic_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/catalog/default/default_table_functions.hpp"

namespace duckdb {

// clang-format off
static const DefaultTableMacro dynamic_sql_examples_table_macros[] = {
    {DEFAULT_SCHEMA, "read_any", {"file_name", nullptr}, {{"format", "'auto'"}, {nullptr, nullptr}}, R"(
----CREATE OR REPLACE MACRO read_any(file_name, format:='auto') AS TABLE (
       WITH "json_case" as (FROM read_json_auto(file_name))
           , "csv_case" as (FROM read_csv(file_name))
           , "parquet_case" as (FROM read_parquet(file_name))
           , "avro_case" as (FROM read_avro(file_name))
           , "blob_case" as (FROM read_blob(file_name))
           , "spatial_case" as (FROM st_read(file_name))
       FROM query_table(
             CASE
               WHEN format=='blob' THEN 'blob_case'
               WHEN format == 'spatial' OR format LIKE 'geo%' OR (format=='auto' AND (file_name ILIKE '%.geojson' OR file_name ILIKE '%.fgb' OR file_name ILIKE '%.prj' OR file_name ILIKE '%.shp')) THEN 'spatial_case'
               WHEN format=='json' OR (format=='auto' AND magic_mime(file_name) ILIKE '%json') OR (file_name LIKE '%.json') THEN 'json_case'
               WHEN format=='csv' OR (format=='auto' AND magic_mime(file_name) ILIKE 'text/plain') THEN 'csv_case'
               WHEN format=='parquet' OR (format=='auto' AND magic_type(file_name) ILIKE 'Apache Parquet%') THEN 'parquet_case'
               WHEN format=='avro' OR (format=='auto' AND magic_type(file_name) ILIKE 'Apache Avro%') THEN 'avro_case'
               WHEN format=='auto' THEN error('read_any can not auto recognize a valid format, try explicitly: FROM read_any("' || file_name ||'", format:="csv"), explcitly supported formats are csv, json, parquet, spatial and blob')
             ELSE error('read_any explicitly provided format is not one of: csv | json | parquet | blob | spatial (or geo alias) | auto"')
             END
       )
----   );
    )"},
	{nullptr, nullptr, {nullptr}, {{nullptr, nullptr}}, nullptr}
	};
// clang-format on

template <bool MIME_TYPE>
struct MagicFunctionLocalState : public FunctionLocalState {
  explicit MagicFunctionLocalState() : FunctionLocalState() {

    /* MAGIC_MIME tells magic to return a mime of the file,
       but you can specify different things	*/

    if (MIME_TYPE) {
      magic_cookie = magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR);
    } else {
      magic_cookie = magic_open(MAGIC_NONE | MAGIC_ERROR);
    }

    if (magic_cookie == NULL) {
      throw std::runtime_error("Unable to initialize magic library");
      return;
    }

#ifndef STATIC_MAGIC_FILE
    if (magic_load(magic_cookie,
                   "/Users/carlo/duckdblabs/extension-template/build/release/"
                   "vcpkg_installed/arm64-osx/share/libmagic/misc/magic.mgc") !=
        0) {
      string message(magic_error(magic_cookie));
      throw std::runtime_error("Cannot load magic database " + message);
    }
#else
    void *buff[1] = {const_cast<unsigned char *>(&magic_mgc[0])};
    size_t z[1] = {size_t(magic_mgc_size)};
    if (magic_load_buffers(magic_cookie, buff, z, 1) != 0) {
      string message(magic_error(magic_cookie));
      throw std::runtime_error("Cannot load magic database " + message);
    }
#endif
  }
  ~MagicFunctionLocalState() {
    if (magic_cookie) {
      magic_close(magic_cookie);
    }
  }
  magic_t magic_cookie;
};

template <bool MIME>
inline unique_ptr<FunctionLocalState>
MagicFunctionLocalStateFun(ExpressionState &state,
                           const BoundFunctionExpression &expr,
                           FunctionData *bind_data) {
  auto res = make_uniq<MagicFunctionLocalState<MIME>>();
  return std::move(res);
}

template <bool MIME>
inline void MagicScalarFun(DataChunk &args, ExpressionState &state,
                           Vector &result) {
  auto &name_vector = args.data[0];
  UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
      name_vector, result, args.size(),
      [&](string_t name, ValidityMask &mask, idx_t idx) {
//if (mask.IsValid(idx) == false) {
//	return string_t();
//}
        auto &localState = ExecuteFunctionState::GetFunctionState(state)
                               ->Cast<MagicFunctionLocalState<MIME>>();

        string terminated = name.GetString();
        const char *actual_file = terminated.c_str();

        const char *magic_full;

#ifndef MAGIC_ON_BUFFER
        const bool onBuffer = true;
#else
        const bool onBuffer = false;
#endif

        if (onBuffer) {
          auto &fs = FileSystem::GetFileSystem(state.GetContext());
          auto handle = fs.OpenFile(actual_file, FileFlags::FILE_FLAGS_READ);
          char buffer[1024] = {};
          auto bytes_read = fs.Read(*handle, buffer, sizeof(buffer) - 1);

          magic_full =
              magic_buffer(localState.magic_cookie, buffer, bytes_read);
        } else {
          magic_full = magic_file(localState.magic_cookie, actual_file);
        }
        if (!magic_full) {
          mask.SetInvalid(idx);
          return string_t();
        }
        string X(magic_full);

        return StringVector::AddString(result, X);
      });
}

static void LoadInternal(ExtensionLoader &loader) {
  loader.SetDescription("Detect file types via magic library");

  auto magic_type_scalar_function =
      ScalarFunction("magic_type", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                     MagicScalarFun<false>, nullptr, nullptr, nullptr,
                     MagicFunctionLocalStateFun<false>);
  loader.RegisterFunction(magic_type_scalar_function);

  auto magic_mime_scalar_function =
      ScalarFunction("magic_mime", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                     MagicScalarFun<true>, nullptr, nullptr, nullptr,
                     MagicFunctionLocalStateFun<true>);
  loader.RegisterFunction(magic_mime_scalar_function);

  // Table Macros
  for (idx_t index = 0;
       dynamic_sql_examples_table_macros[index].name != nullptr; index++) {
    auto table_info = DefaultTableFunctionGenerator::CreateTableMacroInfo(
        dynamic_sql_examples_table_macros[index]);
    loader.RegisterFunction(*table_info);
  }
}

void MagicExtension::Load(ExtensionLoader &loader) { LoadInternal(loader); }
std::string MagicExtension::Name() { return "magic"; }

std::string MagicExtension::Version() const {
#ifdef EXT_VERSION_MAGIC
  return EXT_VERSION_MAGIC;
#else
  return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(magic, loader) {
	duckdb::LoadInternal(loader);
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
