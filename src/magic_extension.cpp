#define DUCKDB_EXTENSION_MAIN

#include <stdio.h>
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
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

namespace duckdb {

struct MagicFunctionLocalState : public FunctionLocalState {
  explicit MagicFunctionLocalState() : FunctionLocalState() {

    /* MAGIC_MIME tells magic to return a mime of the file,
       but you can specify different things	*/
    magic_cookie = magic_open(MAGIC_NONE | MAGIC_ERROR);

    if (magic_cookie == NULL) {
      printf("unable to initialize magic library\n");
      return;
    }

    printf("Loading default magic database\n");

#ifndef STATIC_MAGIC_FILE
    if (magic_load(magic_cookie,
                   "/Users/carlo/duckdblabs/extension-template/build/release/"
                   "vcpkg_installed/arm64-osx/share/libmagic/misc/magic.mgc") !=
        0) {
      printf("cannot load magic database - %s\n", magic_error(magic_cookie));
      magic_close(magic_cookie);
      magic_cookie = NULL;
      return;
    }
#else
    void *buff[1] = {const_cast<unsigned char *>(&magic_mgc[0])};
    size_t z[1] = {size_t(magic_mgc_size)};
    if (magic_load_buffers(magic_cookie, buff, z, 1) != 0) {
      printf("cannot load magic database - %s\n", magic_error(magic_cookie));
      magic_close(magic_cookie);
      magic_cookie = NULL;
      return;
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

inline unique_ptr<FunctionLocalState>
MagicFunctionLocalStateFun(ExpressionState &state,
                           const BoundFunctionExpression &expr,
                           FunctionData *bind_data) {
  auto res = make_uniq<MagicFunctionLocalState>();
  return std::move(res);
}

inline void MagicScalarFun(DataChunk &args, ExpressionState &state,
                           Vector &result) {
  auto &name_vector = args.data[0];
  UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
      name_vector, result, args.size(),
      [&](string_t name, ValidityMask &mask, idx_t idx) {
        auto &localState = ExecuteFunctionState::GetFunctionState(state)
                               ->Cast<MagicFunctionLocalState>();

        string terminated = name.GetString();
        const char *actual_file = terminated.c_str();

        const char *magic_full;

#ifdef MAGIC_ON_BUFFER
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

static void LoadInternal(DatabaseInstance &instance) {
  // Register another scalar function
  auto magic_scalar_function = ScalarFunction(
      "magic", {LogicalType::VARCHAR}, LogicalType::VARCHAR, MagicScalarFun,
      nullptr, nullptr, nullptr, MagicFunctionLocalStateFun);
  ExtensionUtil::RegisterFunction(instance, magic_scalar_function);
}

void MagicExtension::Load(DuckDB &db) { LoadInternal(*db.instance); }
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

DUCKDB_EXTENSION_API void magic_init(duckdb::DatabaseInstance &db) {
  duckdb::DuckDB db_wrapper(db);
  db_wrapper.LoadExtension<duckdb::MagicExtension>();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
