// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <map>
#include <dirent.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

using namespace v8;

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1) return;
  HandleScope scope(args.GetIsolate());
  Local<Value> arg = args[0];
  String::Utf8Value value(arg);
  std::cout << *value << std::endl;
}

static void Throw(v8::Isolate* isolate, std::string message) {
  isolate->ThrowException(
      v8::String::NewFromUtf8(isolate, message.c_str(),
                              v8::NewStringType::kNormal).ToLocalChecked());
}

// This function returns a new array with three elements, x, y, and z.
static Local<Array> ArrayOf(Isolate* isolate, std::vector<Local<Value>> from) {
  // We will be creating temporary handles so we use a handle scope.
  EscapableHandleScope handle_scope(isolate);

  Local<Array> result = Array::New(isolate, from.size());

  int i = 0;
  for (Local<Value> value : from) {
    Maybe<bool> set = result->Set(
        isolate->GetCurrentContext(), i, value);
    assert(!set.IsNothing());
    i++;
  }

  // Return the value through Escape.
  return handle_scope.Escape(result);
}

static Local<String> StringOf(Isolate* isolate, std::string from) {
  return String::NewFromUtf8(
      isolate, from.c_str(), NewStringType::kNormal)
      .ToLocalChecked();
}

static void Put(Isolate* isolate, Local<Object> object, std::string key, Local<Value> value) {
  Local<String> prop = StringOf(isolate, key);
  Maybe<bool> set = object->Set(
      isolate->GetCurrentContext(), prop, value);
  assert(!set.IsNothing());
}

static Local<Value> Get(Isolate* isolate, Local<Object> object, std::string key) {
  return object->Get(
      isolate->GetCurrentContext(), StringOf(isolate, key)).ToLocalChecked();
}

static Local<Object> ObjectOf(Isolate* isolate, std::map<std::string, Local<Value>> from) {
  EscapableHandleScope handle_scope(isolate);

  Local<Object> result = Object::New(isolate);

  for (auto const& property : from) {
    Put(isolate, result, property.first, property.second);
  }

  // Return the value through Escape.
  return handle_scope.Escape(result);
}

static void ReadEntries(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 1) {
    Throw(isolate, "Bad parameters: expecting 1 arg.");
    return;
  }

  std::cout << "... reading entries ..." << std::endl;

  Local<Value> arg = args[0];
  Function* func = Function::Cast(*arg);
  Local<Value> that = v8::Undefined(isolate);

  //Local<Array> files = Array::New(isolate, 1);
  //Maybe<bool> set = files->Set(
  //    isolate->GetCurrentContext(), 0, Integer::New(isolate, 1));
  //assert(!set.IsNothing());

  // file:
  //   - boolean isFile
  //   - boolean isDirectory
  //   - string name
  //   - string fullPath
  //   - getParent(success, error)
  //   - getMetadata(sucesss, error)
  //     - modificationTime
  //     - size

  std::cout << "listing directory" << std::endl;

  struct dirent *entry;
  DIR *dirp = opendir(".");

  std::vector<Local<Value>> files;

  if (dirp) {
    while ((entry = readdir(dirp)) != NULL) {

      Local<Object> file = ObjectOf(isolate, {
          { "isFile", Boolean::New(isolate, entry->d_type == DT_REG) },
          { "isDirectory", Boolean::New(isolate, entry->d_type == DT_DIR) },
          { "name", StringOf(isolate, entry->d_name) },
              });

      files.push_back(file);
      // printf("%s\n", directory->d_name);
    }

    closedir(dirp);
  }

  Local<Value> params[1] = {ArrayOf(isolate, files)};

  MaybeLocal<Value> result = func->Call(isolate->GetCurrentContext(), that, 1, params);

  if (!result.IsEmpty()) {
    // Throw(isolate, "Error calling function");
    return;
  }
}

static void CreateReader(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 1) {
    Throw(isolate, "Bad parameters: expecting 1 arg.");
    return;
  }

  std::cout << "... getting file ..." << std::endl;

  // We will be creating temporary handles so we use a handle scope.
  EscapableHandleScope handle_scope(isolate);

  // DirectoryReader
  Local<ObjectTemplate> reader = ObjectTemplate::New(isolate);
  reader->Set(String::NewFromUtf8(isolate, "readEntries", NewStringType::kNormal)
               .ToLocalChecked(),
               FunctionTemplate::New(isolate, ReadEntries));

  MaybeLocal<Object> result = reader->NewInstance(isolate->GetCurrentContext());

  args.GetReturnValue().Set(result.ToLocalChecked());
}

static void GetFile(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 3) {
    Throw(isolate, "Bad parameters: expecting 3 args.");
    return;
  }

  // We will be creating temporary handles so we use a handle scope.
  EscapableHandleScope handle_scope(isolate);

  // TODO(goto): check the types of the args for errors.
  Function* success = Function::Cast(*args[2]);
  Local<Value> that = v8::Undefined(isolate);
  // NOTE(goto): unsure if this is creating a dangling pointer
  // or if this is leaking the JS memory heap. sanity check,
  // certainly looks fishy to me.
  v8::String::Utf8Value name(args[0]);

  // TODO(goto): check if the file exists. If not, call
  // the error callback as opposed to the success callback.
  Local<Object> file = ObjectOf(isolate, {
      { "isFile", Boolean::New(isolate, true) },
      { "isDirectory", Boolean::New(isolate, false) },
      { "name", StringOf(isolate, *name) }
    });

  Local<Value> params[1] = {file};

  MaybeLocal<Value> result = success->Call(isolate->GetCurrentContext(), that, 1, params);
  if (result.IsEmpty()) {
    // ignores an empty result.
  }
}


// This function returns a new array with three elements, x, y, and z.
Local<Object> FileSystem(Isolate* isolate) {
  // We will be creating temporary handles so we use a handle scope.
  EscapableHandleScope handle_scope(isolate);

  // root
  Local<ObjectTemplate> root = ObjectTemplate::New(isolate);
  root->Set(StringOf(isolate, "createReader"), FunctionTemplate::New(isolate, CreateReader));
  root->Set(StringOf(isolate, "getFile"), FunctionTemplate::New(isolate, GetFile));

  // fs
  Local<ObjectTemplate> fs = ObjectTemplate::New(isolate);
  fs->Set(StringOf(isolate, "root"), root);

  // Return the value through Escape.
  return handle_scope.Escape(
      fs->NewInstance(isolate->GetCurrentContext()).ToLocalChecked());
}

static void RequestFileSystem(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 1) {
    Throw(isolate, "Bad parameters: expecting 1 arg.");
    return;
  }

  Local<Context> context(isolate->GetCurrentContext());
  EscapableHandleScope scope(isolate);

  Local<Value> arg = args[0];
  Function* func = Function::Cast(*arg);
  Local<Value> that = String::NewFromUtf8(
      args.GetIsolate(), "TODO(goto): how do I unset this?",
      NewStringType::kNormal).ToLocalChecked();;
  Local<Value> params[1];

  params[0] = FileSystem(isolate);

  MaybeLocal<Value> result = func->Call(context, that, 1, params);

  if (!result.IsEmpty()) {
    // Throw(isolate, "Error calling function");
    return;
  }
}

static void ReadAsText(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() != 1) {
    Throw(isolate, "expected 1 argument (file)");
    return;
  }

  Object *file = Object::Cast(*args[0]);
  Local<Value> name = file->Get(
      isolate->GetCurrentContext(), StringOf(isolate, "name"))
      .ToLocalChecked();

  if (name.IsEmpty() ||
      name == v8::Undefined(isolate)) {
    Throw(isolate, "invalid file");
    return;
  }

  v8::String::Utf8Value filename(name);

  std::ifstream fd(*filename);

  if (!fd.is_open()) {
    Throw(isolate, "file not found");
    return;
  }

  std::string content((std::istreambuf_iterator<char>(fd)),
                      std::istreambuf_iterator<char>());

  Local<Object> that = args.This();

  Put(isolate, that, "result", StringOf(isolate, content));

  Local<Value> onloadend = Get(isolate, that, "onloadend");

  if (onloadend.IsEmpty() ||
      onloadend == v8::Undefined(isolate)) {
    // TODO(goto): not sure if this is how it is supposed to
    // behave, but, hey, at least it gets the job done :)
    // Wite a comformant implementation later.
    Throw(isolate, "onloadend callback not defined");
    return;
  }

  // TODO(goto): figure out what to do when onloadend is
  // NOT a function!
  Function* callback = Function::Cast(*onloadend);
  Local<Value> params[1];

  // This is the "e" parameter of the onloadevent,
  // since the callback uses this.result instead of e,
  // leaving it empty for now.
  params[0] = ObjectOf(isolate, {
  });

  MaybeLocal<Value> result = callback->Call(
      isolate->GetCurrentContext(), that, 1, params);

  if (!result.IsEmpty()) {
    // Result ignored.
    return;
  }
}

// This function returns a new array with three elements, x, y, and z.
static void FileReader(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  // std::cout << "file reader's constructor" << std::endl;
  Local<Object> that = args.This();
  Put(isolate, that, "foo", StringOf(isolate, "bar"));
  Local<Function> readAsText = FunctionTemplate::New(isolate, ReadAsText)
      ->GetFunction(isolate->GetCurrentContext())
      .ToLocalChecked();
  Put(isolate, that, "readAsText", readAsText);
}

int main(int argc, char* argv[]) {
  // Initialize V8.
  V8::InitializeICUDefaultLocation(argv[0]);
  V8::InitializeExternalStartupData(argv[0]);
  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();

  // Create a new Isolate and make it the current one.
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate);

    // Create a template for the global object and set the
    // built-in global functions.
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);

    // console.log()
    Local<ObjectTemplate> console = ObjectTemplate::New(isolate);
    console->Set(StringOf(isolate, "log"),
                 FunctionTemplate::New(isolate, LogCallback));

    global->Set(StringOf(isolate, "console"), console);

    // requestFileSystem()
    global->Set(StringOf(isolate, "requestFileSystem"),
                FunctionTemplate::New(isolate, RequestFileSystem));

    // new FileReader()
    global->Set(StringOf(isolate, "FileReader"),
                FunctionTemplate::New(isolate, FileReader));

    while (true) {
      // Prints some basic UI.
      std::cout << "user@localhost:~/ " << std::flush;

      // Reads the command from stdin. Leave if we are done.
      std::string input_line;
      if (!getline(std::cin, input_line)) {
        break;
      }

      // Breaks the command line into an array of tokens.
      std::stringstream ss(input_line + ' ');
      std::string item;
      std::vector<std::string> command;
      // TODO(goto): this doesn't work well with "", like
      // echo "foo bar" will return [echo, "foo, bar"]
      // as opposed to [echo, "foo bar"]
      while (std::getline(ss, item, ' ')) {
        command.push_back(item);
      }

      // Loads the contents of the command into a file.
      // The first token is used as the program name;
      std::ifstream file(command[0] + ".js");

      // Complains if it doesn't exist and carry on.
      if (!file.is_open()) {
        std::cout << "web.sh: " << input_line << ": command not found" << std::endl;
        continue;
      }

      std::string source((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

      // Create a new context.
      Local<Context> context = Context::New(isolate, NULL, global);

      // Enter the context for compiling and running the hello world script.
      Context::Scope context_scope(context);

      // Create a string containing the JavaScript source code.
      Local<String> code =
          String::NewFromUtf8(isolate, source.c_str(),
                              NewStringType::kNormal).ToLocalChecked();

      // Compile the source code.
      TryCatch trycatch(isolate);

      MaybeLocal<Script> script = Script::Compile(context, code);

      if (script.IsEmpty()) {
        Local<Value> exception = trycatch.Exception();
        String::Utf8Value exception_str(exception);
        std::cout<< "Exception: " << *exception_str << std::endl;
        continue;
      }

      // Run the script to get the result.
      Local<Value> result;
      if (!script.ToLocalChecked()->Run(context).ToLocal(&result)) {
        Local<Value> exception = trycatch.Exception();
        String::Utf8Value exception_str(exception);
        std::cout << "Exception running script: " << *exception_str << std::endl;
        continue;
      }

      Local<Value> main;
      if (!context->Global()->Get(context, StringOf(isolate, "main"))
          .ToLocal(&main) &&
          !main->IsFunction()) {
        std::cout << "main() not found" << std::endl;
        continue;
      }

      // calls main() witht the command line arguments.
      Local<Value> that = v8::Undefined(isolate);
      std::vector<Local<Value>> commandline;
      for (auto const& arg: command) {
        commandline.push_back(StringOf(isolate, arg));
      }
      Local<Object> argv = ArrayOf(isolate, commandline);
      Local<Value> params[1] = {argv};

      Local<Value> value;
      if (!Function::Cast(*main)->Call(
              isolate->GetCurrentContext(), that, 1, params).ToLocal(&value)) {
        Local<Value> exception = trycatch.Exception();
        String::Utf8Value exception_str(exception);
        std::cout << "Exception running main(): " << *exception_str << std::endl;
        continue;
      }
   }
  }

  std::cout << "Exiting." << std::endl;

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  delete platform;
  delete create_params.array_buffer_allocator;
  return 0;
}
