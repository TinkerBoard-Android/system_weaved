// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_dictionary.h"

#include <gtest/gtest.h>

#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateDictionaryValue;

TEST(CommandDictionary, Empty) {
  buffet::CommandDictionary dict;
  EXPECT_TRUE(dict.IsEmpty());
  EXPECT_EQ(nullptr, dict.FindCommand("robot.jump"));
  EXPECT_TRUE(dict.GetCommandNamesByCategory("robotd").empty());
}

TEST(CommandDictionary, LoadCommands) {
  auto json = CreateDictionaryValue(R"({
    'robot': {
      'jump': {
        'parameters': {
          'height': 'integer',
          '_jumpType': ['_withAirFlip', '_withSpin', '_withKick']
        },
        'results': {}
      }
    }
  })");
  buffet::CommandDictionary dict;
  EXPECT_TRUE(dict.LoadCommands(*json, "robotd", nullptr, nullptr));
  EXPECT_EQ(1, dict.GetSize());
  EXPECT_NE(nullptr, dict.FindCommand("robot.jump"));
  json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {}
      },
      'shutdown': {
        'parameters': {},
        'results': {}
      }
    }
  })");
  EXPECT_TRUE(dict.LoadCommands(*json, "powerd", nullptr, nullptr));
  EXPECT_EQ(3, dict.GetSize());
  EXPECT_NE(nullptr, dict.FindCommand("robot.jump"));
  EXPECT_NE(nullptr, dict.FindCommand("base.reboot"));
  EXPECT_NE(nullptr, dict.FindCommand("base.shutdown"));
  EXPECT_EQ(nullptr, dict.FindCommand("foo.bar"));
  std::vector<std::string> expected_commands{"base.reboot", "base.shutdown"};
  EXPECT_EQ(expected_commands, dict.GetCommandNamesByCategory("powerd"));
}

TEST(CommandDictionary, LoadCommands_Failures) {
  buffet::CommandDictionary dict;
  chromeos::ErrorPtr error;

  // Command definition missing 'parameters' property.
  auto json = CreateDictionaryValue("{'robot':{'jump':{'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", nullptr, &error));
  EXPECT_EQ("parameter_missing", error->GetCode());
  EXPECT_EQ("Command definition 'robot.jump' is missing property 'parameters'",
            error->GetMessage());
  error.reset();

  // Command definition missing 'results' property.
  json = CreateDictionaryValue("{'robot':{'jump':{'parameters':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", nullptr, &error));
  EXPECT_EQ("parameter_missing", error->GetCode());
  EXPECT_EQ("Command definition 'robot.jump' is missing property 'results'",
            error->GetMessage());
  error.reset();

  // Command definition is not an object.
  json = CreateDictionaryValue("{'robot':{'jump':0}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", nullptr, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  EXPECT_EQ("Expecting an object for command 'jump'", error->GetMessage());
  error.reset();

  // Package definition is not an object.
  json = CreateDictionaryValue("{'robot':'blah'}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", nullptr, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  EXPECT_EQ("Expecting an object for package 'robot'", error->GetMessage());
  error.reset();

  // Invalid command definition is not an object.
  json = CreateDictionaryValue(
      "{'robot':{'jump':{'parameters':{'flip':0},'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", nullptr, &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  EXPECT_EQ("Invalid definition for command 'robot.jump'", error->GetMessage());
  EXPECT_NE(nullptr, error->GetInnerError());  // Must have additional info.
  error.reset();

  // Empty command name.
  json = CreateDictionaryValue("{'robot':{'':{'parameters':{},'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", nullptr, &error));
  EXPECT_EQ("invalid_command_name", error->GetCode());
  EXPECT_EQ("Unnamed command encountered in package 'robot'",
            error->GetMessage());
  error.reset();
}

TEST(CommandDictionaryDeathTest, LoadCommands_RedefineInDifferentCategory) {
  // Redefine commands in different category.
  buffet::CommandDictionary dict;
  chromeos::ErrorPtr error;
  auto json = CreateDictionaryValue(
      "{'robot':{'jump':{'parameters':{},'results':{}}}}");
  dict.LoadCommands(*json, "category1", nullptr, &error);
  ASSERT_DEATH(dict.LoadCommands(*json, "category2", nullptr, &error),
               ".*Definition for command 'robot.jump' overrides an "
               "earlier definition in category 'category1'");
}

TEST(CommandDictionary, LoadCommands_CustomCommandNaming) {
  // Custom command must start with '_'.
  buffet::CommandDictionary base_dict;
  buffet::CommandDictionary dict;
  chromeos::ErrorPtr error;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {}
      }
    }
  })");
  base_dict.LoadCommands(*json, "", nullptr, &error);
  EXPECT_TRUE(dict.LoadCommands(*json, "robotd", &base_dict, &error));
  auto json2 = CreateDictionaryValue(
      "{'base':{'jump':{'parameters':{},'results':{}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json2, "robotd", &base_dict, &error));
  EXPECT_EQ("invalid_command_name", error->GetCode());
  EXPECT_EQ("The name of custom command 'jump' in package 'base' must start "
            "with '_'", error->GetMessage());
  error.reset();

  // If the command starts with "_", then it's Ok.
  json2 = CreateDictionaryValue(
      "{'base':{'_jump':{'parameters':{},'results':{}}}}");
  EXPECT_TRUE(dict.LoadCommands(*json2, "robotd", &base_dict, nullptr));
}

TEST(CommandDictionary, LoadCommands_RedefineStdCommand) {
  // Redefine commands parameter type.
  buffet::CommandDictionary base_dict;
  buffet::CommandDictionary dict;
  chromeos::ErrorPtr error;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {'version': 'integer'}
      }
    }
  })");
  base_dict.LoadCommands(*json, "", nullptr, &error);

  auto json2 = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'string'},
        'results': {'version': 'integer'}
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json2, "robotd", &base_dict, &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  EXPECT_EQ("Invalid definition for command 'base.reboot'",
            error->GetMessage());
  EXPECT_EQ("invalid_parameter_definition", error->GetInnerError()->GetCode());
  EXPECT_EQ("Error in definition of property 'delay'",
            error->GetInnerError()->GetMessage());
  EXPECT_EQ("param_type_changed", error->GetFirstError()->GetCode());
  EXPECT_EQ("Redefining a property of type integer as string",
            error->GetFirstError()->GetMessage());
  error.reset();

  auto json3 = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {'version': 'string'}
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json3, "robotd", &base_dict, &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  EXPECT_EQ("Invalid definition for command 'base.reboot'",
            error->GetMessage());
  // TODO(antonm): remove parameter from error below and use some generic.
  EXPECT_EQ("invalid_parameter_definition", error->GetInnerError()->GetCode());
  EXPECT_EQ("Error in definition of property 'version'",
            error->GetInnerError()->GetMessage());
  EXPECT_EQ("param_type_changed", error->GetFirstError()->GetCode());
  EXPECT_EQ("Redefining a property of type integer as string",
            error->GetFirstError()->GetMessage());
  error.reset();
}

TEST(CommandDictionary, GetCommandsAsJson) {
  auto json_base = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': {'maximum': 100}},
        'results': {}
      },
      'shutdown': {
        'parameters': {},
        'results': {}
      }
    }
  })");
  buffet::CommandDictionary base_dict;
  base_dict.LoadCommands(*json_base, "base", nullptr, nullptr);

  auto json = buffet::unittests::CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': {'minimum': 10}},
        'results': {}
      }
    },
    'robot': {
      '_jump': {
        'parameters': {'_height': 'integer'},
        'results': {}
      }
    }
  })");
  buffet::CommandDictionary dict;
  dict.LoadCommands(*json, "device", &base_dict, nullptr);

  json = dict.GetCommandsAsJson(false, nullptr);
  EXPECT_NE(nullptr, json.get());
  EXPECT_EQ("{'base':{'reboot':{'parameters':{'delay':{'minimum':10}}}},"
            "'robot':{'_jump':{'parameters':{'_height':'integer'}}}}",
            buffet::unittests::ValueToString(json.get()));

  json = dict.GetCommandsAsJson(true, nullptr);
  EXPECT_NE(nullptr, json.get());
  EXPECT_EQ("{'base':{'reboot':{'parameters':{'delay':{"
            "'maximum':100,'minimum':10,'type':'integer'}}}},"
            "'robot':{'_jump':{'parameters':{'_height':{'type':'integer'}}}}}",
            buffet::unittests::ValueToString(json.get()));
}

TEST(CommandDictionary, LoadCommandsWithVisibility) {
  buffet::CommandDictionary dict;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'command1': {
        'parameters': {},
        'results': {},
        'visibility':''
      },
      'command2': {
        'parameters': {},
        'results': {},
        'visibility':'local'
      },
      'command3': {
        'parameters': {},
        'results': {},
        'visibility':'cloud'
      },
      'command4': {
        'parameters': {},
        'results': {},
        'visibility':'all'
      },
      'command5': {
        'parameters': {},
        'results': {},
        'visibility':'cloud,local'
      }
    }
  })");
  EXPECT_TRUE(dict.LoadCommands(*json, "testd", nullptr, nullptr));
  auto cmd = dict.FindCommand("base.command1");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("none", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command2");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("local", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command3");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("cloud", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command4");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command5");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
}

TEST(CommandDictionary, LoadCommandsWithVisibility_Inheritance) {
  buffet::CommandDictionary base_dict;
  auto json = CreateDictionaryValue(R"({
    'base': {
      'command1': {
        'parameters': {},
        'results': {},
        'visibility':''
      },
      'command2': {
        'parameters': {},
        'results': {},
        'visibility':'local'
      },
      'command3': {
        'parameters': {},
        'results': {},
        'visibility':'cloud'
      },
      'command4': {
        'parameters': {},
        'results': {},
        'visibility':'all'
      },
      'command5': {
        'parameters': {},
        'results': {},
        'visibility':'local,cloud'
      }
    }
  })");
  EXPECT_TRUE(base_dict.LoadCommands(*json, "testd", nullptr, nullptr));

  buffet::CommandDictionary dict;
  json = CreateDictionaryValue(R"({
    'base': {
      'command1': {
        'parameters': {},
        'results': {}
      },
      'command2': {
        'parameters': {},
        'results': {}
      },
      'command3': {
        'parameters': {},
        'results': {}
      },
      'command4': {
        'parameters': {},
        'results': {}
      },
      'command5': {
        'parameters': {},
        'results': {}
      },
      '_command6': {
        'parameters': {},
        'results': {}
      }
    }
  })");
  EXPECT_TRUE(dict.LoadCommands(*json, "testd", &base_dict, nullptr));

  auto cmd = dict.FindCommand("base.command1");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("none", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command2");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("local", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command3");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("cloud", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command4");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base.command5");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());

  cmd = dict.FindCommand("base._command6");
  ASSERT_NE(nullptr, cmd);
  EXPECT_EQ("all", cmd->GetVisibility().ToString());
}

TEST(CommandDictionary, LoadCommandsWithVisibility_Failures) {
  buffet::CommandDictionary dict;
  chromeos::ErrorPtr error;

  auto json = CreateDictionaryValue(R"({
    'base': {
      'jump': {
        'parameters': {},
        'results': {},
        'visibility':'foo'
      }
    }
  })");
  EXPECT_FALSE(dict.LoadCommands(*json, "testd", nullptr, &error));
  EXPECT_EQ("invalid_command_visibility", error->GetCode());
  EXPECT_EQ("Error parsing command 'base.jump'", error->GetMessage());
  EXPECT_EQ("invalid_parameter_value", error->GetInnerError()->GetCode());
  EXPECT_EQ("Invalid command visibility value 'foo'",
            error->GetInnerError()->GetMessage());
  error.reset();
}
