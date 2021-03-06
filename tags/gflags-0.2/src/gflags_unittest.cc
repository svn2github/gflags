// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Marius Eriksen
//
// For now, this unit test does not cover all features of
// commandlineflags.cc

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>     // for unlink()
#include <sys/stat.h>   // for mkdir()
#include <math.h>       // for isinf() and isnan()
#include <vector>
#include <string>
#include "google/gflags.h"

using std::vector;
using std::string;

DECLARE_string(tryfromenv);   // in commandlineflags.cc

DEFINE_string(test_tmpdir, "/tmp/gflags_unittest", "Dir we use for temp files");

DEFINE_bool(test_bool, false, "tests bool-ness");
DEFINE_int32(test_int32, -1, "");
DEFINE_int64(test_int64, -2, "");
DEFINE_uint64(test_uint64, 2, "");
DEFINE_double(test_double, -1.0, "");
DEFINE_string(test_string, "initial", "");

//
// The below ugliness gets some additional code coverage in the -helpxml
// and -helpmatch test cases having to do with string lengths and formatting
//
DEFINE_bool(test_bool_with_quite_quite_quite_quite_quite_quite_quite_quite_quite_quite_quite_quite_quite_quite_long_name,
            false,
            "extremely_extremely_extremely_extremely_extremely_extremely_extremely_extremely_long_meaning");

DEFINE_string(test_str1, "initial", "");
DEFINE_string(test_str2, "initial", "");
DEFINE_string(test_str3, "initial", "");

// This is used to test setting tryfromenv manually
DEFINE_string(test_tryfromenv, "initial", "");

// These are never used in this unittest, but can be used by
// commandlineflags_unittest.sh when it needs to specify flags
// that are legal for commandlineflags_unittest but don't need to
// be a particular value.
DEFINE_bool(unused_bool, true, "unused bool-ness");
DEFINE_int32(unused_int32, -1001, "");
DEFINE_int64(unused_int64, -2001, "");
DEFINE_uint64(unused_uint64, 2000, "");
DEFINE_double(unused_double, -1000.0, "");
DEFINE_string(unused_string, "unused", "");

_START_GOOGLE_NAMESPACE_

// The following is some bare-bones testing infrastructure

#define EXPECT_TRUE(cond)                               \
  do {                                                  \
    if (!(cond)) {                                      \
      fprintf(stderr, "Check failed: %s\n", #cond);     \
      exit(1);                                          \
    }                                                   \
  } while (0)

#define EXPECT_FALSE(cond)  EXPECT_TRUE(!(cond))

#define EXPECT_OP(op, val1, val2)                                       \
  do {                                                                  \
    if (!((val1) op (val2))) {                                          \
      fprintf(stderr, "Check failed: %s %s %s\n", #val1, #op, #val2);   \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

#define EXPECT_EQ(val1, val2)  EXPECT_OP(==, val1, val2)
#define EXPECT_NE(val1, val2)  EXPECT_OP(!=, val1, val2)
#define EXPECT_GT(val1, val2)  EXPECT_OP(>, val1, val2)
#define EXPECT_LT(val1, val2)  EXPECT_OP(<, val1, val2)

#define EXPECT_PRED1(pred, arg)                                 \
  do {                                                          \
    if (!((pred)(arg))) {                                       \
      fprintf(stderr, "Check failed: %s(%s)\n", #pred, #arg);   \
      exit(1);                                                  \
    }                                                           \
  } while (0)

#define EXPECT_DOUBLE_EQ(val1, val2)                                    \
  do {                                                                  \
    if (((val1) < (val2) - 0.001 || (val1) > (val2) + 0.001)) {         \
      fprintf(stderr, "Check failed: %s == %s\n", #val1, #val2);        \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

#define EXPECT_STREQ(val1, val2)                                        \
  do {                                                                  \
    if (strcmp((val1), (val2)) != 0) {                                  \
      fprintf(stderr, "Check failed: streq(%s, %s)\n", #val1, #val2);   \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

static bool g_called_exit;
static void CalledExit(int) { g_called_exit = true; }

#define EXPECT_DEATH(fn, msg)                                           \
  do {                                                                  \
    g_called_exit = false;                                              \
    extern void (*commandlineflags_exitfunc)(int);   /* in gflags.cc */ \
    commandlineflags_exitfunc = &CalledExit;                            \
    fn;                                                                 \
    commandlineflags_exitfunc = &exit;    /* set back to its default */ \
    if (!g_called_exit) {                                               \
      fprintf(stderr, "Function didn't die (%s): %s\n", msg, #fn);      \
      exit(1);                                                          \
    }                                                                   \
  } while (0)


vector<void (*)()> g_testlist;  // the tests to run

#define TEST(a, b)                                      \
  struct Test_##a##_##b {                               \
    Test_##a##_##b() { g_testlist.push_back(&Run); }    \
    static void Run() { FlagSaver fs; RunTest(); }      \
    static void RunTest();                              \
  };                                                    \
  static Test_##a##_##b g_test_##a##_##b;               \
  void Test_##a##_##b::RunTest()


static int RUN_ALL_TESTS() {
  vector<void (*)()>::const_iterator it;
  for (it = g_testlist.begin(); it != g_testlist.end(); ++it) {
    (*it)();
  }
  fprintf(stderr, "Passed %d tests\n\nPASS\n", (int)g_testlist.size());
  return 0;
}


// Death tests for "help" options.
//
// The help system automatically calls exit(1) when you specify any of
// the help-related flags ("-helpmatch", "-helpxml") so we can't test
// those mainline.

// Tests that "-helpmatch" causes the process to die.
TEST(ReadFlagsFromStringDeathTest, HelpMatch) {
  EXPECT_DEATH(ReadFlagsFromString("-helpmatch=base", GetArgv0(), true),
               "");
}


// Tests that "-helpxml" causes the process to die.
TEST(ReadFlagsFromStringDeathTest, HelpXml) {
  EXPECT_DEATH(ReadFlagsFromString("-helpxml", GetArgv0(), true),
               "");
}


// A subroutine needed for testing reading flags from a string.
void TestFlagString(const string& flags,
                    const string& expected_string,
                    bool expected_bool,
                    int32 expected_int32,
                    double expected_double) {
  EXPECT_TRUE(ReadFlagsFromString(flags,
                                  GetArgv0(),
                                  // errors are fatal
                                  true));

  EXPECT_EQ(expected_string, FLAGS_test_string);
  EXPECT_EQ(expected_bool, FLAGS_test_bool);
  EXPECT_EQ(expected_int32, FLAGS_test_int32);
  EXPECT_DOUBLE_EQ(expected_double, FLAGS_test_double);
}


// Tests reading flags from a string.
TEST(FlagFileTest, ReadFlagsFromString) {
  TestFlagString(
      // Flag string
      "-test_string=continued\n"
      "# some comments are in order\n"
      "# some\n"
      "  # comments\n"
      "#are\n"
      "                  #trickier\n"
      "# than others\n"
      "-test_bool=true\n"
      "     -test_int32=1\n"
      "-test_double=0.0\n",
      // Expected values
      "continued",
      true,
      1,
      0.0);

  TestFlagString(
      // Flag string
      "# let's make sure it can update values\n"
      "-test_string=initial\n"
      "-test_bool=false\n"
      "-test_int32=123\n"
      "-test_double=123.0\n",
      // Expected values
      "initial",
      false,
      123,
      123.0);
}

// Tests the filename part of the flagfile
TEST(FlagFileTest, FilenamesOurfileLast) {
  FLAGS_test_string = "initial";
  FLAGS_test_bool = false;
  FLAGS_test_int32 = -1;
  FLAGS_test_double = -1.0;
  TestFlagString(
      // Flag string
      "-test_string=continued\n"
      "# some comments are in order\n"
      "# some\n"
      "  # comments\n"
      "#are\n"
      "                  #trickier\n"
      "# than others\n"
      "not_our_filename\n"
      "-test_bool=true\n"
      "     -test_int32=1\n"
      "gflags_unittest\n"
      "-test_double=1000.0\n",
      // Expected values
      "continued",
      false,
      -1,
      1000.0);
}

TEST(FlagFileTest, FilenamesOurfileFirst) {
  FLAGS_test_string = "initial";
  FLAGS_test_bool = false;
  FLAGS_test_int32 = -1;
  FLAGS_test_double = -1.0;
  TestFlagString(
      // Flag string
      "-test_string=continued\n"
      "# some comments are in order\n"
      "# some\n"
      "  # comments\n"
      "#are\n"
      "                  #trickier\n"
      "# than others\n"
      "gflags_unittest\n"
      "-test_bool=true\n"
      "     -test_int32=1\n"
      "not_our_filename\n"
      "-test_double=1000.0\n",
      // Expected values
      "continued",
      true,
      1,
      -1.0);
}

TEST(FlagFileTest, FilenamesOurfileGlob) {
  FLAGS_test_string = "initial";
  FLAGS_test_bool = false;
  FLAGS_test_int32 = -1;
  FLAGS_test_double = -1.0;
  TestFlagString(
      // Flag string
      "-test_string=continued\n"
      "# some comments are in order\n"
      "# some\n"
      "  # comments\n"
      "#are\n"
      "                  #trickier\n"
      "# than others\n"
      "*flags*\n"
      "-test_bool=true\n"
      "     -test_int32=1\n"
      "flags\n"
      "-test_double=1000.0\n",
      // Expected values
      "continued",
      true,
      1,
      -1.0);
}

TEST(FlagFileTest, FilenamesOurfileInBigList) {
  FLAGS_test_string = "initial";
  FLAGS_test_bool = false;
  FLAGS_test_int32 = -1;
  FLAGS_test_double = -1.0;
  TestFlagString(
      // Flag string
      "-test_string=continued\n"
      "# some comments are in order\n"
      "# some\n"
      "  # comments\n"
      "#are\n"
      "                  #trickier\n"
      "# than others\n"
      "*first* *flags* *third*\n"
      "-test_bool=true\n"
      "     -test_int32=1\n"
      "flags\n"
      "-test_double=1000.0\n",
      // Expected values
      "continued",
      true,
      1,
      -1.0);
}

// Tests that a failed flag-from-string read keeps flags at default values
TEST(FlagFileTest, FailReadFlagsFromString) {
  FLAGS_test_int32 = 119;
  string flags("# let's make sure it can update values\n"
               "-test_string=non_initial\n"
               "-test_bool=false\n"
               "-test_int32=123\n"
               "-test_double=illegal\n");

  EXPECT_FALSE(ReadFlagsFromString(flags,
                                   GetArgv0(),
                                   // errors are fatal
                                   false));

  EXPECT_EQ(119, FLAGS_test_int32);
  EXPECT_EQ("initial", FLAGS_test_string);
}

// Tests that flags can be set to ordinary values.
TEST(SetFlagValueTest, OrdinaryValues) {
  EXPECT_EQ("initial", FLAGS_test_str1);

  SetCommandLineOptionWithMode("test_str1", "second", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("second", FLAGS_test_str1);  // set; was default

  SetCommandLineOptionWithMode("test_str1", "third", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("second", FLAGS_test_str1);  // already set once

  FLAGS_test_str1 = "initial";
  SetCommandLineOptionWithMode("test_str1", "third", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("initial", FLAGS_test_str1);  // still already set before

  SetCommandLineOptionWithMode("test_str1", "third", SET_FLAGS_VALUE);
  EXPECT_EQ("third", FLAGS_test_str1);  // changed value

  SetCommandLineOptionWithMode("test_str1", "fourth", SET_FLAGS_DEFAULT);
  EXPECT_EQ("third", FLAGS_test_str1);
  // value not changed (already set before)

  EXPECT_EQ("initial", FLAGS_test_str2);

  SetCommandLineOptionWithMode("test_str2", "second", SET_FLAGS_DEFAULT);
  EXPECT_EQ("second", FLAGS_test_str2);  // changed (was default)

  FLAGS_test_str2 = "extra";
  EXPECT_EQ("extra", FLAGS_test_str2);

  FLAGS_test_str2 = "second";
  SetCommandLineOptionWithMode("test_str2", "third", SET_FLAGS_DEFAULT);
  EXPECT_EQ("third", FLAGS_test_str2);  // still changed (was equal to default)

  SetCommandLineOptionWithMode("test_str2", "fourth", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("fourth", FLAGS_test_str2);  // changed (was default)

  EXPECT_EQ("initial", FLAGS_test_str3);

  SetCommandLineOptionWithMode("test_str3", "second", SET_FLAGS_DEFAULT);
  EXPECT_EQ("second", FLAGS_test_str3);  // changed

  FLAGS_test_str3 = "third";
  SetCommandLineOptionWithMode("test_str3", "fourth", SET_FLAGS_DEFAULT);
  EXPECT_EQ("third", FLAGS_test_str3);  // not changed (was set)

  SetCommandLineOptionWithMode("test_str3", "fourth", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("third", FLAGS_test_str3);  // not changed (was set)

  SetCommandLineOptionWithMode("test_str3", "fourth", SET_FLAGS_VALUE);
  EXPECT_EQ("fourth", FLAGS_test_str3);  // changed value
}


// Tests that flags can be set to exceptional values.
TEST(SetFlagValueTest, ExceptionalValues) {
  EXPECT_EQ("test_double set to inf\n",
            SetCommandLineOption("test_double", "inf"));
  EXPECT_PRED1(isinf, FLAGS_test_double);

  EXPECT_EQ("test_double set to inf\n",
            SetCommandLineOption("test_double", "INF"));
  EXPECT_PRED1(isinf, FLAGS_test_double);

  // set some bad values
  EXPECT_EQ("",
            SetCommandLineOption("test_double", "0.1xxx"));
  EXPECT_EQ("",
            SetCommandLineOption("test_double", " "));
  EXPECT_EQ("",
            SetCommandLineOption("test_double", ""));
  EXPECT_EQ("test_double set to -inf\n",
            SetCommandLineOption("test_double", "-inf"));
  EXPECT_PRED1(isinf, FLAGS_test_double);
  EXPECT_GT(0, FLAGS_test_double);

  EXPECT_EQ("test_double set to nan\n",
            SetCommandLineOption("test_double", "NaN"));
  EXPECT_PRED1(isnan, FLAGS_test_double);
}

// Tests that integer flags can be specified in many ways
TEST(SetFlagValueTest, DifferentRadices) {
  EXPECT_EQ("test_int32 set to 12\n",
            SetCommandLineOption("test_int32", "12"));

  EXPECT_EQ("test_int32 set to 16\n",
            SetCommandLineOption("test_int32", "0x10"));

  EXPECT_EQ("test_int32 set to 34\n",
            SetCommandLineOption("test_int32", "0X22"));

  // Leading 0 is *not* octal; it's still decimal
  EXPECT_EQ("test_int32 set to 10\n",
            SetCommandLineOption("test_int32", "010"));
}

// Tests what happens when you try to set a flag to an illegal value
TEST(SetFlagValueTest, IllegalValues) {
  FLAGS_test_bool = true;
  FLAGS_test_int32 = 119;
  FLAGS_test_int64 = 1191;
  FLAGS_test_uint64 = 11911;

  EXPECT_EQ("",
            SetCommandLineOption("test_bool", "12"));

  EXPECT_EQ("",
            SetCommandLineOption("test_int32", "7000000000000"));

  // TODO(csilvers): uncomment this when we disallow negative numbers for uint64
#if 0
  EXPECT_EQ("",
            SetCommandLineOption("test_uint64", "-1"));
#endif

  EXPECT_EQ("",
            SetCommandLineOption("test_int64", "not a number!"));

  // Test the empty string with each type of input
  EXPECT_EQ("", SetCommandLineOption("test_bool", ""));
  EXPECT_EQ("", SetCommandLineOption("test_int32", ""));
  EXPECT_EQ("", SetCommandLineOption("test_int64", ""));
  EXPECT_EQ("", SetCommandLineOption("test_uint64", ""));
  EXPECT_EQ("", SetCommandLineOption("test_double", ""));
  EXPECT_EQ("test_string set to \n", SetCommandLineOption("test_string", ""));

  EXPECT_EQ(true, FLAGS_test_bool);
  EXPECT_EQ(119, FLAGS_test_int32);
  EXPECT_EQ(1191, FLAGS_test_int64);
  EXPECT_EQ(11911, FLAGS_test_uint64);
}


// Tests that the FooFromEnv does the right thing
TEST(FromEnvTest, LegalValues) {
  setenv("BOOL_VAL1", "true", 1);
  setenv("BOOL_VAL2", "false", 1);
  setenv("BOOL_VAL3", "1", 1);
  setenv("BOOL_VAL4", "F", 1);
  EXPECT_EQ(true, BoolFromEnv("BOOL_VAL1", false));
  EXPECT_EQ(false, BoolFromEnv("BOOL_VAL2", true));
  EXPECT_EQ(true, BoolFromEnv("BOOL_VAL3", false));
  EXPECT_EQ(false, BoolFromEnv("BOOL_VAL4", true));
  EXPECT_EQ(true, BoolFromEnv("BOOL_VAL_UNKNOWN", true));
  EXPECT_EQ(false, BoolFromEnv("BOOL_VAL_UNKNOWN", false));

  setenv("INT_VAL1", "1", 1);
  setenv("INT_VAL2", "-1", 1);
  EXPECT_EQ(1, Int32FromEnv("INT_VAL1", 10));
  EXPECT_EQ(-1, Int32FromEnv("INT_VAL2", 10));
  EXPECT_EQ(10, Int32FromEnv("INT_VAL_UNKNOWN", 10));

  setenv("INT_VAL3", "1099511627776", 1);
  EXPECT_EQ(1, Int64FromEnv("INT_VAL1", 20));
  EXPECT_EQ(-1, Int64FromEnv("INT_VAL2", 20));
  EXPECT_EQ(1099511627776LL, Int64FromEnv("INT_VAL3", 20));
  EXPECT_EQ(20, Int64FromEnv("INT_VAL_UNKNOWN", 20));

  EXPECT_EQ(1, Uint64FromEnv("INT_VAL1", 30));
  EXPECT_EQ(1099511627776ULL, Uint64FromEnv("INT_VAL3", 30));
  EXPECT_EQ(30, Uint64FromEnv("INT_VAL_UNKNOWN", 30));

  // I pick values here that can be easily represented exactly in floating-point
  setenv("DOUBLE_VAL1", "0.0", 1);
  setenv("DOUBLE_VAL2", "1.0", 1);
  setenv("DOUBLE_VAL3", "-1.0", 1);
  EXPECT_EQ(0.0, DoubleFromEnv("DOUBLE_VAL1", 40.0));
  EXPECT_EQ(1.0, DoubleFromEnv("DOUBLE_VAL2", 40.0));
  EXPECT_EQ(-1.0, DoubleFromEnv("DOUBLE_VAL3", 40.0));
  EXPECT_EQ(40.0, DoubleFromEnv("DOUBLE_VAL_UNKNOWN", 40.0));

  setenv("STRING_VAL1", "", 1);
  setenv("STRING_VAL2", "my happy string!", 1);
  EXPECT_STREQ("", StringFromEnv("STRING_VAL1", "unknown"));
  EXPECT_STREQ("my happy string!", StringFromEnv("STRING_VAL2", "unknown"));
  EXPECT_STREQ("unknown", StringFromEnv("STRING_VAL_UNKNOWN", "unknown"));
}

// Tests that the FooFromEnv dies on parse-error
TEST(FromEnvTest, IllegalValues) {
  setenv("BOOL_BAD1", "so true!",1 );
  setenv("BOOL_BAD2", "", 1);
  EXPECT_DEATH(BoolFromEnv("BOOL_BAD1", false), "error parsing env variable");
  EXPECT_DEATH(BoolFromEnv("BOOL_BAD2", true), "error parsing env variable");

  setenv("INT_BAD1", "one", 1);
  setenv("INT_BAD2", "100000000000000000", 1);
  setenv("INT_BAD3", "0xx10", 1);
  setenv("INT_BAD4", "", 1);
  EXPECT_DEATH(Int32FromEnv("INT_BAD1", 10), "error parsing env variable");
  EXPECT_DEATH(Int32FromEnv("INT_BAD2", 10), "error parsing env variable");
  EXPECT_DEATH(Int32FromEnv("INT_BAD3", 10), "error parsing env variable");
  EXPECT_DEATH(Int32FromEnv("INT_BAD4", 10), "error parsing env variable");

  setenv("BIGINT_BAD1", "18446744073709551616000", 1);
  EXPECT_DEATH(Int64FromEnv("INT_BAD1", 20), "error parsing env variable");
  EXPECT_DEATH(Int64FromEnv("INT_BAD3", 20), "error parsing env variable");
  EXPECT_DEATH(Int64FromEnv("INT_BAD4", 20), "error parsing env variable");
  EXPECT_DEATH(Int64FromEnv("BIGINT_BAD1", 200), "error parsing env variable");

  setenv("BIGINT_BAD2", "-1", 1);
  EXPECT_DEATH(Uint64FromEnv("INT_BAD1", 30), "error parsing env variable");
  EXPECT_DEATH(Uint64FromEnv("INT_BAD3", 30), "error parsing env variable");
  EXPECT_DEATH(Uint64FromEnv("INT_BAD4", 30), "error parsing env variable");
  EXPECT_DEATH(Uint64FromEnv("BIGINT_BAD1", 30), "error parsing env variable");
  // TODO(csilvers): uncomment this when we disallow negative numbers for uint64
#if 0
  EXPECT_DEATH(Uint64FromEnv("BIGINT_BAD2", 30), "error parsing env variable");
#endif

  setenv("DOUBLE_BAD1", "0.0.0", 1);
  setenv("DOUBLE_BAD2", "", 1);
  EXPECT_DEATH(DoubleFromEnv("DOUBLE_BAD1", 40.0), "error parsing env variable");
  EXPECT_DEATH(DoubleFromEnv("DOUBLE_BAD2", 40.0), "error parsing env variable");
}

// Tests that FlagSaver can save the states of string flags.
TEST(FlagSaverTest, CanSaveStringFlagStates) {
  // 1. Initializes the flags.

  // State of flag test_str1:
  //   default value - "initial"
  //   current value - "initial"
  //   not set       - true

  SetCommandLineOptionWithMode("test_str2", "second", SET_FLAGS_VALUE);
  // State of flag test_str2:
  //   default value - "initial"
  //   current value - "second"
  //   not set       - false

  SetCommandLineOptionWithMode("test_str3", "second", SET_FLAGS_DEFAULT);
  // State of flag test_str3:
  //   default value - "second"
  //   current value - "second"
  //   not set       - true

  // 2. Saves the flag states.

  {
    FlagSaver fs;

    // 3. Modifies the flag states.

    SetCommandLineOptionWithMode("test_str1", "second", SET_FLAGS_VALUE);
    EXPECT_EQ("second", FLAGS_test_str1);
    // State of flag test_str1:
    //   default value - "second"
    //   current value - "second"
    //   not set       - true

    SetCommandLineOptionWithMode("test_str2", "third", SET_FLAGS_DEFAULT);
    EXPECT_EQ("second", FLAGS_test_str2);
    // State of flag test_str2:
    //   default value - "third"
    //   current value - "second"
    //   not set       - false

    SetCommandLineOptionWithMode("test_str3", "third", SET_FLAGS_VALUE);
    EXPECT_EQ("third", FLAGS_test_str3);
    // State of flag test_str1:
    //   default value - "second"
    //   current value - "third"
    //   not set       - false

    // 4. Restores the flag states.
  }

  // 5. Verifies that the states were restored.

  // Verifies that the value of test_str1 was restored.
  EXPECT_EQ("initial", FLAGS_test_str1);
  // Verifies that the "not set" attribute of test_str1 was restored to true.
  SetCommandLineOptionWithMode("test_str1", "second", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("second", FLAGS_test_str1);

  // Verifies that the value of test_str2 was restored.
  EXPECT_EQ("second", FLAGS_test_str2);
  // Verifies that the "not set" attribute of test_str2 was restored to false.
  SetCommandLineOptionWithMode("test_str2", "fourth", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("second", FLAGS_test_str2);

  // Verifies that the value of test_str3 was restored.
  EXPECT_EQ("second", FLAGS_test_str3);
  // Verifies that the "not set" attribute of test_str3 was restored to true.
  SetCommandLineOptionWithMode("test_str3", "fourth", SET_FLAG_IF_DEFAULT);
  EXPECT_EQ("fourth", FLAGS_test_str3);
}


// Tests that FlagSaver can save the values of various-typed flags.
TEST(FlagSaverTest, CanSaveVariousTypedFlagValues) {
  // Initializes the flags.
  FLAGS_test_bool = false;
  FLAGS_test_int32 = -1;
  FLAGS_test_int64 = -2;
  FLAGS_test_uint64 = 3;
  FLAGS_test_double = 4.0;
  FLAGS_test_string = "good";

  // Saves the flag states.
  {
    FlagSaver fs;

    // Modifies the flags.
    FLAGS_test_bool = true;
    FLAGS_test_int32 = -5;
    FLAGS_test_int64 = -6;
    FLAGS_test_uint64 = 7;
    FLAGS_test_double = 8.0;
    FLAGS_test_string = "bad";

    // Restores the flag states.
  }

  // Verifies the flag values were restored.
  EXPECT_FALSE(FLAGS_test_bool);
  EXPECT_EQ(-1, FLAGS_test_int32);
  EXPECT_EQ(-2, FLAGS_test_int64);
  EXPECT_EQ(3, FLAGS_test_uint64);
  EXPECT_DOUBLE_EQ(4.0, FLAGS_test_double);
  EXPECT_EQ("good", FLAGS_test_string);
}

TEST(GetAllFlagsTest, BaseTest) {
  vector<CommandLineFlagInfo> flags;
  GetAllFlags(&flags);
  bool found_test_bool = false;
  vector<CommandLineFlagInfo>::const_iterator i;
  for (i = flags.begin(); i != flags.end(); ++i) {
    if (i->name == "test_bool") {
      found_test_bool = true;
      EXPECT_EQ(i->type, "bool");
      EXPECT_EQ(i->default_value, "false");
      break;
    }
  }
  EXPECT_EQ(true, found_test_bool);
}

TEST(ShowUsageWithFlagsTest, BaseTest) {
  // TODO(csilvers): test this by allowing output other than to stdout.
  // Not urgent since this functionality is tested via
  // gflags_unittest.sh, though only through use of --help.
}

TEST(ShowUsageWithFlagsRestrictTest, BaseTest) {
  // TODO(csilvers): test this by allowing output other than to stdout.
  // Not urgent since this functionality is tested via
  // gflags_unittest.sh, though only through use of --helpmatch.
}

// Note: all these argv-based tests depend on SetArgv being called
// before InitGoogle() in main(), below.
TEST(GetArgvsTest, BaseTest) {
  vector<string> argvs = GetArgvs();
  EXPECT_EQ(4, argvs.size());
  EXPECT_EQ("/test/argv/for/gflags_unittest", argvs[0]);
  EXPECT_EQ("argv 2", argvs[1]);
  EXPECT_EQ("3rd argv", argvs[2]);
  EXPECT_EQ("argv #4", argvs[3]);
}

TEST(GetArgvTest, BaseTest) {
  EXPECT_STREQ("/test/argv/for/gflags_unittest "
               "argv 2 3rd argv argv #4", GetArgv());
}

TEST(GetArgv0Test, BaseTest) {
  EXPECT_STREQ("/test/argv/for/gflags_unittest", GetArgv0());
}

TEST(GetArgvSumTest, BaseTest) {
  // This number is just the sum of the ASCII values of all the chars
  // in GetArgv().
  EXPECT_EQ(4904, GetArgvSum());
}

TEST(ProgramInvocationNameTest, BaseTest) {
  EXPECT_STREQ("/test/argv/for/gflags_unittest",
               ProgramInvocationName());
}

TEST(ProgramInvocationShortNameTest, BaseTest) {
  EXPECT_STREQ("gflags_unittest", ProgramInvocationShortName());
}

TEST(ProgramUsageTest, BaseTest) { // Depends on 1st arg to InitGoogle in main()
  EXPECT_STREQ("/test/argv/for/gflags_unittest: "
               "<useless flag> [...]\nDoes something useless.\n",
               ProgramUsage());
}

TEST(GetCommandLineOptionTest, NameExistsAndIsDefault) {
  string value("will be changed");
  bool is_default;
  bool r = GetCommandLineOption("test_bool", &value, &is_default);
  EXPECT_EQ(true, r);
  EXPECT_EQ("false", value);
  EXPECT_EQ(true, is_default);

  r = GetCommandLineOption("test_int32", &value, &is_default);
  EXPECT_EQ(true, r);
  EXPECT_EQ("-1", value);
  EXPECT_EQ(true, is_default);
}

TEST(GetCommandLineOptionTest, NameExistsAndWasAssigned) {
  FLAGS_test_int32 = 400;
  string value("will be changed");
  bool is_default;
  const bool r = GetCommandLineOption("test_int32", &value, &is_default);
  EXPECT_EQ(true, r);
  EXPECT_EQ("400", value);
  EXPECT_EQ(false, is_default);
}

TEST(GetCommandLineOptionTest, NameExistsAndWasSet) {
  SetCommandLineOption("test_int32", "700");
  string value("will be changed");
  bool is_default;
  const bool r = GetCommandLineOption("test_int32", &value, &is_default);
  EXPECT_EQ(true, r);
  EXPECT_EQ("700", value);
  EXPECT_EQ(false, is_default);
}

TEST(GetCommandLineOptionTest, NameExistsAndWasNotSet) {
  // This doesn't set the flag's value, but rather its default value.
  // is_default is still true, but the 'default' value returned has changed!
  SetCommandLineOptionWithMode("test_int32", "800", SET_FLAGS_DEFAULT);
  string value("will be changed");
  bool is_default;
  const bool r = GetCommandLineOption("test_int32", &value, &is_default);
  EXPECT_EQ(true, r);
  EXPECT_EQ("800", value);
  EXPECT_EQ(true, is_default);
}

TEST(GetCommandLineOptionTest, NameExistsAndWasConditionallySet) {
  SetCommandLineOptionWithMode("test_int32", "900", SET_FLAG_IF_DEFAULT);
  string value("will be changed");
  bool is_default;
  const bool r = GetCommandLineOption("test_int32", &value, &is_default);
  EXPECT_EQ(true, r);
  EXPECT_EQ("900", value);
  EXPECT_EQ(false, is_default);
}

TEST(GetCommandLineOptionTest, NameDoesNotExist) {
  string value("will not be changed");
  bool is_default = false;
  const bool r = GetCommandLineOption("test_int3210", &value, &is_default);
  EXPECT_EQ(false, r);
  EXPECT_EQ("will not be changed", value);
  EXPECT_EQ(false, is_default);
}

TEST(GetCommandLineOptionTest, NoLastArg) {
  // Mostly, makes sure passing in NULL as last arg doesn't cause a crash
  string value("will not be changed, at first");
  bool r = GetCommandLineOption("test_int3210", &value, NULL);
  EXPECT_EQ(false, r);
  EXPECT_EQ("will not be changed, at first", value);

  r = GetCommandLineOption("test_int32", &value, NULL);
  EXPECT_EQ(true, r);
  EXPECT_EQ("-1", value);
}

TEST(GetCommandLineFlagInfoTest, FlagExists) {
  CommandLineFlagInfo info;
  bool r = GetCommandLineFlagInfo("test_int32", &info);
  EXPECT_EQ(true, r);
  EXPECT_EQ("test_int32", info.name);
  EXPECT_EQ("int32", info.type);
  EXPECT_EQ("", info.description);
  EXPECT_EQ("-1", info.default_value);
  r = GetCommandLineFlagInfo("test_bool", &info);
  EXPECT_EQ(true, r);
  EXPECT_EQ("test_bool", info.name);
  EXPECT_EQ("bool", info.type);
  EXPECT_EQ("tests bool-ness", info.description);
  EXPECT_EQ("false", info.default_value);
}

TEST(GetCommandLineFlagInfoTest, FlagDoesNotExist) {
  CommandLineFlagInfo info;
  bool r = GetCommandLineFlagInfo("test_int3210", &info);
  EXPECT_EQ(false, r);
}


// These are lightly tested because they're deprecated.  Basically,
// the tests are meant to cover how existing users use these functions,
// but not necessarily how new users could use them.
TEST(DeprecatedFunctionsTest, CommandlineFlagsIntoString) {
  string s = CommandlineFlagsIntoString();
  EXPECT_NE(string::npos, s.find("--test_bool="));
}

TEST(DeprecatedFunctionsTest, AppendFlagsIntoFile) {
  FLAGS_test_int32 = 10;     // just to make the test more interesting
  string filename(FLAGS_test_tmpdir + "/flagfile");
  unlink(filename.c_str());  // just to be safe
  const bool r = AppendFlagsIntoFile(filename, "not the real argv0");
  EXPECT_EQ(true, r);

  FILE* fp = fopen(filename.c_str(), "r");
  EXPECT_TRUE(fp != NULL);
  char line[8192];
  fgets(line, sizeof(line)-1, fp);   // first line should be progname
  EXPECT_STREQ("not the real argv0\n", line);

  bool found_bool = false, found_int32 = false;
  while (fgets(line, sizeof(line)-1, fp)) {
    line[sizeof(line)-1] = '\0';    // just to be safe
    if (strcmp(line, "--test_bool=false\n") == 0)
      found_bool = true;
    if (strcmp(line, "--test_int32=10\n") == 0)
      found_int32 = true;
  }
  EXPECT_EQ(true, found_int32);
  EXPECT_EQ(true, found_bool);
  fclose(fp);
}

TEST(DeprecatedFunctionsTest, ReadFromFlagsFile) {
  FLAGS_test_int32 = -10;    // just to make the test more interesting
  string filename(FLAGS_test_tmpdir + "/flagfile2");
  unlink(filename.c_str());  // just to be safe
  bool r = AppendFlagsIntoFile(filename, GetArgv0());
  EXPECT_EQ(true, r);

  FLAGS_test_int32 = -11;
  r = ReadFromFlagsFile(filename, GetArgv0(), true);
  EXPECT_EQ(true, r);
  EXPECT_EQ(-10, FLAGS_test_int32);
} // unnamed namespace

TEST(DeprecatedFunctionsTest, ReadFromFlagsFileFailure) {
  FLAGS_test_int32 = -20;
  string filename(FLAGS_test_tmpdir + "/flagfile3");
  FILE* fp = fopen(filename.c_str(), "w");
  EXPECT_TRUE(fp != NULL);
  // Note the error in the bool assignment below...
  fprintf(fp, "%s\n--test_int32=-21\n--test_bool=not_a_bool!\n", GetArgv0());
  fclose(fp);

  FLAGS_test_int32 = -22;
  const bool r = ReadFromFlagsFile(filename, GetArgv0(), false);
  EXPECT_EQ(false, r);
  EXPECT_EQ(-22, FLAGS_test_int32);   // the -21 from the flagsfile didn't take
}

TEST(FlagsSetBeforeInitGoogleTest, TryFromEnv) {
  EXPECT_EQ("pre-set", FLAGS_test_tryfromenv);
}

static int Main(int argc, char **argv) {
  // We need to call SetArgv before InitGoogle, so our "test" argv will
  // win out over this executable's real argv.  That makes running this
  // test with a real --help flag kinda annoying, unfortunately.
  const char* test_argv[] = { "/test/argv/for/gflags_unittest",
                              "argv 2", "3rd argv", "argv #4" };
  SetArgv(sizeof(test_argv)/sizeof(*test_argv), test_argv);

  // The first arg is the usage message, also important for testing.
  string usage_message = (string(GetArgv0()) +
                          ": <useless flag> [...]\nDoes something useless.\n");

  // We test setting tryfromenv manually, and making sure initgoogle still
  // evaluates it.
  FLAGS_tryfromenv = "test_tryfromenv";
  setenv("FLAGS_test_tryfromenv", "pre-set", 1);

  SetUsageMessage(usage_message.c_str());
  ParseCommandLineFlags(&argc, &argv, true);

  mkdir(FLAGS_test_tmpdir.c_str(), 0755);

  return RUN_ALL_TESTS();
}

_END_GOOGLE_NAMESPACE_

int main(int argc, char** argv) {
  return GOOGLE_NAMESPACE::Main(argc, argv);
}
