/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Defaults_hpp
#define Defaults_hpp

static constexpr int DEFAULT_CLIENT_PORT = 55055;
static constexpr int DEFAULT_SERVER_PORT = 55056;

#ifndef JUCE_WINDOWS
static const String SERVER_CONFIG_FILE = "~/.audiogridderserver";
static const String PLUGIN_CONFIG_FILE = "~/.audiogridder";
static const String KNOWN_PLUGINS_FILE = "~/.audiogridderserver.cache";
static const String DEAD_MANS_FILE = "~/.audiogridderserver.crash";
static const String SERVER_RUN_FILE = "~/.audiogridderserver.running";
#else
static const String SERVER_CONFIG_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver";
static const String PLUGIN_CONFIG_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridder";
static const String KNOWN_PLUGINS_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver.cache";
static const String DEAD_MANS_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver.crash";
static const String SERVER_RUN_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver.running";
#endif

static constexpr int DEFAULT_NUM_OF_BUFFERS = 8;
static constexpr int DEFAULT_NUM_RECENTS = 10;
static constexpr int DEFAULT_LOAD_PLUGIN_TIMEOUT = 15;

static constexpr uint32 DEFAULT_BG_COLOR = 0xff222222;
static constexpr uint32 DEFAULT_BUTTON_COLOR = 0xff333333;
static constexpr uint32 DEFAULT_SLIDERTRACK_COLOR = 0xffffc13b;
static constexpr uint32 DEFAULT_SLIDERTHUMB_COLOR = 0xaaffffff;
static constexpr uint32 DEFAULT_SLIDERBG_COLOR = 0xff606060;
static constexpr uint32 DEFAULT_ACTIVE_COLOR = 0xffffc13b;
static constexpr uint32 DEFAULT_CPU_LOW_COLOR = 0xff00ff00;
static constexpr uint32 DEFAULT_CPU_MEDIUM_COLOR = 0xffffff00;
static constexpr uint32 DEFAULT_CPU_HIGH_COLOR = 0xffff0000;

static const String MDNS_SERVICE_NAME = "_audiogridder._tcp.local.";

#endif /* Defaults_hpp */
