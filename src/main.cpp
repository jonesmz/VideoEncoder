/*
 * Copyright © 2021 Michael Jones
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "string_concat.h"

// <ryml_std.hpp> is only needed if interop with std types is desired.
//  ryml itself does not use any STL container.
#include <ryml_std.hpp> // optional header. BUT when used, needs to be included BEFORE ryml.hpp
#include <ryml.hpp>

#include <boost/program_options.hpp>

#include <string>
#include <random>
#include <memory>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <string_view>

#include <cstdio>
#include <cstdint>

namespace po = boost::program_options;

static constexpr auto sc_commandPrefix ="#!/bin/sh\nset -v\nset -x\n";

// sc_commandPrefix and sc_commandSuffix must be concatinated
// with a string that defines the bash variables INPUT, OUTPUT, and ARGS
// Note the newline at the beginning of sc_commandSuffix!!!
static constexpr auto sc_commandSuffix =
R"(
    #include "cmd_suffix.sh"
)";


template<typename DIR_ITERATOR>
std::map<std::filesystem::path, ryml::Tree> build_yaml_config_map(std::filesystem::path const& path)
{
    std::map<std::filesystem::path, ryml::Tree> configs;
    for(auto const& item : DIR_ITERATOR(path, std::filesystem::directory_options::follow_directory_symlink))
    {
        if(std::filesystem::file_status const& status = item.status();
              ! std::filesystem::exists(status)
           || ! std::filesystem::is_regular_file(status))
        {
            continue;
        }

        std::filesystem::path itemPath = item.path();
        if(itemPath.filename() != "VideoEncoder.yaml")
        {
            continue;
        }

        try
        {
            std::ifstream t(itemPath);
            std::string const fileContents((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
            auto tree = ryml::parse_in_arena(ryml::to_csubstr(itemPath.generic_string()), ryml::to_csubstr(fileContents));
            auto const& [it, success] = configs.emplace(std::move(item), std::move(tree));
            if( ! success)
            {
                throw std::invalid_argument(string_concat("Duplicate config file : ", it->first.generic_string()));
            }
        }
        catch(std::exception const& ex)
        {
            throw std::invalid_argument(string_concat("Error parsing config : ", itemPath.generic_string(), "\n", ex.what()));
        }
    }
    return configs;
}


struct Command
{
    Command(std::filesystem::path in,
            std::filesystem::path out,
            std::string arg)
     : input(std::move(in))
     , output(std::move(out))
     , arguments(std::move(arg))
    { }

    Command(Command &&) = default;
    Command(Command const&) = default;

    Command& operator=(Command &&) = default;
    Command& operator=(Command const&) = default;

    std::filesystem::path input;
    std::filesystem::path output;
    std::string arguments;
};

template<typename YAML_T, typename KEY_T, typename CURRENT_T>
auto append_string_if_child_exists(YAML_T const& yml, KEY_T const& key, CURRENT_T const& current)
{
    if(yml.has_child(key))
    {
        return string_concat(current, yml[key].val());
    }
    else
    {
        return current;
    }
}

template<typename YAML_T, typename KEY_T, typename CURRENT_T>
auto append_fspath_if_child_exists(YAML_T const& yml, KEY_T const& key, CURRENT_T const& current)
{
    if(yml.has_child(key))
    {
        auto const& range = yml[key].val();
        return current / std::string(range.begin(), range.end());
    }
    else
    {
        return current;
    }
}

void parse_yaml_config(std::filesystem::path const& source,
                       std::filesystem::path const& destination,
                       ryml::Tree const& config,
                       std::vector<Command> & commands)
{
    for(auto const& topLevel : config.rootref())
    {
        std::filesystem::path topLevelSource      = append_fspath_if_child_exists(topLevel, "Source",          source);
        std::filesystem::path topLevelDestination = append_fspath_if_child_exists(topLevel, "Destination",     destination);
        std::string           topLevelName        = append_string_if_child_exists(topLevel, "Name",            std::string());
        std::string           topLevelEncodeOpts  = append_string_if_child_exists(topLevel, "EncodingOptions", std::string());

        if(topLevel.has_child("Seasons"))
        {
            // YAML Layout for TV shows with seasons
            for(auto const& season : topLevel["Seasons"])
            {
                std::filesystem::path seasonSource       = append_fspath_if_child_exists(season, "Source",          topLevelSource);
                std::filesystem::path seasonDestination  = append_fspath_if_child_exists(season, "Destination",     topLevelDestination);
                std::string           seasonName         = append_string_if_child_exists(season, "Name",            topLevelName);
                std::string           seasonEncodeOpts   = append_string_if_child_exists(season, "EncodingOptions", topLevelEncodeOpts);

                for(auto const& intakeFile : season["IntakeFiles"])
                {
                    std::filesystem::path intakeFileSource      = append_fspath_if_child_exists(intakeFile, "Source",          seasonSource);
                    std::filesystem::path intakeFileDestination = append_fspath_if_child_exists(intakeFile, "Destination",     seasonDestination);
                    std::string           intakeFileName        = append_string_if_child_exists(intakeFile, "Name",            seasonName);
                    std::string           intakeFileEncodeOpts  = append_string_if_child_exists(intakeFile, "EncodingOptions", seasonEncodeOpts);

                    for(auto const& episode : intakeFile["Episodes"])
                    {

                        std::filesystem::path episodeSource      = append_fspath_if_child_exists(episode, "Source",          intakeFileSource);
                        std::filesystem::path episodeDestination = append_fspath_if_child_exists(episode, "Destination",     intakeFileDestination);
                        std::string           episodeEncodeOpts  = append_string_if_child_exists(episode, "EncodingOptions", intakeFileEncodeOpts);
                        std::string           episodeName        = append_string_if_child_exists(episode, "Name",            intakeFileName);
                        episodeDestination /= string_concat(episodeName, " - ", season["Season"].val(), episode["Episode"].val(), " - ", episode["Title"].val(), ".m4v");

                        commands.emplace_back(std::move(episodeSource),
                                              std::move(episodeDestination),
                                              std::move(episodeEncodeOpts));
                    }
                }
            }
        }

        // Yaml layout for groupings of movies
        if(topLevel.has_child("Movies"))
        {
            for(auto const& movie : topLevel["Movies"])
            {
                std::filesystem::path movieSource       = append_fspath_if_child_exists(movie, "Source",          topLevelSource);
                std::filesystem::path movieDestination  = append_fspath_if_child_exists(movie, "Destination",     topLevelDestination);
                std::string           movieName         = append_string_if_child_exists(movie, "Name",            topLevelName);
                std::string           movieEncodeOpts   = append_string_if_child_exists(movie, "EncodingOptions", topLevelEncodeOpts);

                for(auto const& intakeFile : movie["IntakeFiles"])
                {
                    std::filesystem::path intakeFileSource      = append_fspath_if_child_exists(intakeFile, "Source",          movieSource);
                    std::filesystem::path intakeFileDestination = append_fspath_if_child_exists(intakeFile, "Destination",     movieDestination);
                    std::string           intakeFileName        = append_string_if_child_exists(intakeFile, "Name",            movieName);
                    std::string           intakeFileEncodeOpts  = append_string_if_child_exists(intakeFile, "EncodingOptions", movieEncodeOpts);

                    for(auto const& output : intakeFile["Outputs"])
                    {
                        std::filesystem::path outputSource      = append_fspath_if_child_exists(output, "Source",          intakeFileSource);
                        std::filesystem::path outputDestination = append_fspath_if_child_exists(output, "Destination",     intakeFileDestination);
                        std::string           outputEncodeOpts  = append_string_if_child_exists(output, "EncodingOptions", intakeFileEncodeOpts);
                        std::string           outputName        = append_string_if_child_exists(output, "Name",            intakeFileName);
                        outputDestination /= string_concat(outputName, movie["Movie"].val(), ".m4v");

                        commands.emplace_back(std::move(outputSource),
                                              std::move(outputDestination),
                                              std::move(outputEncodeOpts));
                    }
                }
            }
        }
    }
}

int main(int argc, char ** argv)
{
    bool recursive;
    std::string preset;
    std::filesystem::path source;
    std::filesystem::path destination;

    {
        bool help = false;
        // Declare the supported options.
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help",        po::bool_switch(&help)->default_value(false), "Produce this help message")
            ("preset",      po::value(&preset)->default_value("Chromecast 1080p30 Surround"), "Handbrake preset to use for encoding")
            ("source",      po::value(&source)->default_value(std::filesystem::current_path()), "Path to directory where VideoEncoder configuration files are located.")
            ("destination", po::value(&destination)->default_value(std::filesystem::current_path()), "Path to directory to output encoded videos.")
            ("recursive",   po::bool_switch(&recursive)->default_value(true), "Search for config files recursively")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if(help)
        {
            std::cout << desc << std::endl;
            return 1;
        }
    }

    std::vector<Command> commands;

    // Parse
    for([[maybe_unused]] auto && [path, config]
          : recursive
          ? build_yaml_config_map<std::filesystem::recursive_directory_iterator>(source)
          : build_yaml_config_map<std::filesystem::directory_iterator>(source))
    {
        parse_yaml_config(path.parent_path(), destination, std::move(config), commands);
    }

    for(auto const& cmd : commands)
    {
        auto const& command = string_concat(sc_commandPrefix,
                                            "\nINPUT=\"",  cmd.input.generic_string(),  "\"",
                                            "\nOUTPUT=\"", cmd.output.generic_string(), "\"",
                                            "\nARGS=\"",   cmd.arguments,               "\"",
                                            "\nPRESET=\"", preset,                      "\"",
                                            sc_commandSuffix);
        //std::cout << command;
        system(command.c_str());
    }
    return 0;
}
