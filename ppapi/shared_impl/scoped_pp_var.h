// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SCOPED_PP_VAR_H_
#define PPAPI_SHARED_IMPL_SCOPED_PP_VAR_H_

#include <stdlib.h>

#include "base/macros.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT ScopedPPVar {
 public:
  struct PassRef {};

  ScopedPPVar();

  // Takes one reference to the given var.
  explicit ScopedPPVar(const PP_Var& v);

  // Assumes responsibility for one ref that the var already has.
  ScopedPPVar(const PassRef&, const PP_Var& v);

  // Implicit copy constructor allowed.
  ScopedPPVar(const ScopedPPVar& other);

  ~ScopedPPVar();

  ScopedPPVar& operator=(const PP_Var& r);
  ScopedPPVar& operator=(const ScopedPPVar& other) {
    return operator=(other.var_);
  }

  const PP_Var& get() const { return var_; }

  // Returns the PP_Var, passing the reference to the caller. This class
  // will no longer hold the var.
  PP_Var Release();

 private:
  PP_Var var_;
};

// An array of PP_Vars which will be deallocated and have their references
// decremented when they go out of scope.
class ScopedPPVarArray {
 public:
  struct PassPPBMemoryAllocatedArray {};

  // Assumes responsibility for one ref of each of the vars in the array as
  // well as the array memory allocated by PPB_Memory_Dev.
  // TODO(raymes): Add compatibility for arrays allocated with C++ "new".
  ScopedPPVarArray(const PassPPBMemoryAllocatedArray&,
                   PP_Var* array,
                   size_t size);

  explicit ScopedPPVarArray(size_t size);
  ~ScopedPPVarArray();

  // Passes ownership of the vars and the underlying array memory to the caller.
  // Note that the memory has been allocated with PPB_Memory_Dev.
  PP_Var* Release(const PassPPBMemoryAllocatedArray&, size_t* size);

  PP_Var* get() { return array_; }
  size_t size() { return size_; }

  // Adds a ref to |var|. The refcount of the existing var will be decremented.
  void Set(size_t index, PP_Var var);
  const PP_Var& operator[](size_t index) { return array_[index]; }

 private:
  // TODO(raymes): Consider supporting copy/assign.
  DISALLOW_COPY_AND_ASSIGN(ScopedPPVarArray);

  PP_Var* array_;
  size_t size_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SCOPED_PP_VAR_H_
