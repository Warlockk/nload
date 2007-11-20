/***************************************************************************
                                  main.cpp
                             -------------------
    begin                : Wed Jul 25 2001
    copyright            : (C) 2001 - 2007 by Roland Riegel 
    email                : feedback@roland-riegel.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/*
 * nload
 * real time monitor for network traffic
 * Copyright (C) 2001 - 2007 Roland Riegel <feedback@roland-riegel.de>
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include "device.h"
#include "devreader.h"
#include "devreaderfactory.h"
#include "graph.h"
#include "main.h"
#include "screen.h"
#include "setting.h"
#include "settingstore.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <ctype.h>
#include <time.h>
#include <curses.h>
#include <signal.h>

#define STANDARD_AVERAGE_WINDOW 300
#define STANDARD_DATA_FORMAT Statistics::megaByte
#define STANDARD_HIDE_GRAPHS false
#define STANDARD_MAX_DEFLECTION 10240
#define STANDARD_SLEEP_INTERVAL 500
#define STANDARD_TRAFFIC_FORMAT Statistics::kiloBit

using namespace std;

static OptWindow m_optWindow;
static TrafficWindow m_mainWindow;

int main(int argc, char *argv[])
{
    SettingStore::add(Setting("sleep_interval", "Refresh interval (ms)", STANDARD_SLEEP_INTERVAL));
    SettingStore::add(Setting("multiple_devices", "Show multiple devices", STANDARD_HIDE_GRAPHS));
    SettingStore::add(Setting("bar_max_in", "Max Incoming deflection (kBit/s)", STANDARD_MAX_DEFLECTION));
    SettingStore::add(Setting("bar_max_out", "Max Outgoing deflection (kBit/s)", STANDARD_MAX_DEFLECTION));
    SettingStore::add(Setting("average_window", "Window length for average (s)", STANDARD_AVERAGE_WINDOW));
    SettingStore::add(Setting("traffic_format", "Unit for traffic numbers", STANDARD_TRAFFIC_FORMAT));
    SettingStore::add(Setting("data_format", "Unit for data numbers", STANDARD_DATA_FORMAT));

    map<string, string> valueMapping;

    valueMapping[toString(false)] = "[ ]";
    valueMapping[toString(true)] = "[x]";
    SettingStore::get("multiple_devices").setValueMapping(valueMapping);
    valueMapping.clear();

    valueMapping[toString(Statistics::humanReadableBit)] = "Human Readable (Bit)";
    valueMapping[toString(Statistics::humanReadableByte)] = "Human Readable (Byte)";
    valueMapping[toString(Statistics::bit)] = "Bit";
    valueMapping[toString(Statistics::byte)] = "Byte";
    valueMapping[toString(Statistics::kiloBit)] = "kBit";
    valueMapping[toString(Statistics::kiloByte)] = "kByte";
    valueMapping[toString(Statistics::megaBit)] = "MBit";
    valueMapping[toString(Statistics::megaByte)] = "MByte";
    valueMapping[toString(Statistics::gigaBit)] = "GBit";
    valueMapping[toString(Statistics::gigaByte)] = "GByte";
    SettingStore::get("traffic_format").setValueMapping(valueMapping);
    SettingStore::get("data_format").setValueMapping(valueMapping);
    valueMapping.clear();

    list<string> devicesRequested;
    bool print_only_once = false;

    // parse the command line
    for(int i = 1; i < argc; i++)
    {
        // does the user want help?
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printHelp(false);
            exit(0);
        }
        // has the user set a non-default average time window?
        else if(strcmp(argv[i], "-a") == 0)
        {
            Setting& setting = SettingStore::get("average_window");
            
            if(i < argc - 1 && isdigit(argv[ i + 1 ][0]) != 0)
            {
                setting = atoi(argv[ i + 1 ]);
                if(setting < 1)
                    setting = STANDARD_AVERAGE_WINDOW;

                i++;
            }
            else
            {
                cerr << "Wrong argument for the -a parameter." << endl;
                printHelp(true);
                exit(1);
            }
        }
        // has the user set a non-default 100% mark for
        // the incoming bandwidth bar?
        else if(strcmp(argv[i], "-i") == 0)
        {
            Setting& setting = SettingStore::get("bar_max_in");
            
            if(i < argc - 1 && isdigit(argv[ i + 1 ][0]) != 0)
            {
                setting = atol(argv[ i + 1 ]);
                if(setting == 0)
                    setting = STANDARD_MAX_DEFLECTION;

                i++;
            }
            else
            {
                cerr << "Wrong argument for the -i parameter." << endl;
                printHelp(true);
                exit(1);
            }
        }
        // has the user set a non-default 100% mark for
        // the outgoing bandwidth bar?
        else if(strcmp(argv[i], "-o") == 0)
        {
            Setting& setting = SettingStore::get("bar_max_out");
            
            if(i < argc - 1 && isdigit(argv[ i + 1 ][0]) != 0)
            {
                setting = atol(argv[ i + 1 ]);
                if(setting == 0)
                    setting = STANDARD_MAX_DEFLECTION;

                i++;
            }
            else
            {
                cerr << "Wrong argument for the -o parameter." << endl;
                printHelp(true);
                exit(1);
            }
        }
        // has the user set a non-default refresh interval?
        else if(strcmp(argv[i], "-t") == 0)
        {
            Setting& setting = SettingStore::get("sleep_interval");
            
            if(i < argc - 1 && isdigit(argv[ i + 1 ][0]) != 0)
            {
                setting = atoi(argv[ i + 1 ]);
                if(setting == 0)
                {
                    print_only_once = true;
                    setting = STANDARD_SLEEP_INTERVAL;
                }

                i++;
            }
            else
            {
                cerr << "Wrong argument for the -t parameter." << endl;
                printHelp(true);
                exit(1);
            }
        }
        // has the user set a non-default unit for traffic numbers?
        else if(strcmp(argv[i], "-u") == 0)
        {
            Setting& setting = SettingStore::get("traffic_format");
            
            if(i < argc - 1 && isalpha(argv[ i + 1 ][0]) != 0)
            {
                switch(argv[ i + 1 ][0])
                {
                    case 'H':
                        setting = Statistics::humanReadableByte;
                        break;
                    case 'h':
                        setting = Statistics::humanReadableBit;
                        break;
                    case 'B':
                        setting = Statistics::byte;
                        break;
                    case 'b':
                        setting = Statistics::bit;
                        break;
                    case 'K':
                        setting = Statistics::kiloByte;
                        break;
                    case 'k':
                        setting = Statistics::kiloBit;
                        break;
                    case 'M':
                        setting = Statistics::megaByte;
                        break;
                    case 'm':
                        setting = Statistics::megaBit;
                        break;
                    case 'G':
                        setting = Statistics::gigaByte;
                        break;
                    case 'g':
                        setting = Statistics::gigaBit;
                        break;
                    default:
                        cerr << "Wrong argument for the -u parameter." << endl;
                        printHelp(true);
                        exit(1);
                }

                i++;
            }
            else
            {
                cerr << "Wrong argument for the -u parameter." << endl;
                printHelp(true);
                exit(1);
            }
        }
        // has the user set a non-default unit for numbers of amount of data?
        else if(strcmp(argv[i], "-U") == 0)
        {
            Setting& setting = SettingStore::get("data_format");
            
            if(i < argc - 1 && isalpha(argv[ i + 1 ][0]) != 0)
            {
                switch(argv[ i + 1 ][0])
                {
                    case 'H':
                        setting = Statistics::humanReadableByte;
                        break;
                    case 'h':
                        setting = Statistics::humanReadableBit;
                        break;
                    case 'B':
                        setting = Statistics::byte;
                        break;
                    case 'b':
                        setting = Statistics::bit;
                        break;
                    case 'K':
                        setting = Statistics::kiloByte;
                        break;
                    case 'k':
                        setting = Statistics::kiloBit;
                        break;
                    case 'M':
                        setting = Statistics::megaByte;
                        break;
                    case 'm':
                        setting = Statistics::megaBit;
                        break;
                    case 'G':
                        setting = Statistics::gigaByte;
                        break;
                    case 'g':
                        setting = Statistics::gigaBit;
                        break;
                    default:
                        cerr << "Wrong argument for the -U parameter." << endl;
                        printHelp(true);
                        exit(1);
                }

                i++;
            }
            else
            {
                cerr << "Wrong argument for the -U parameter." << endl;
                printHelp(true);
                exit(1);
            }
        
        }
        // has the user chosen to display multiple devices and thus not to display graphs?
        else if(strcmp(argv[i], "-m") == 0)
        {
            SettingStore::get("multiple_devices") = true;
        }
        // obsolete -b option
        else if(strcmp(argv[i], "-b") == 0)
        {
        }
        // obsolete -s option
        else if(strcmp(argv[i], "-s") == 0)
        {
        }
        // assume unknown parameter to be the network device
        else
        {
            devicesRequested.push_back(argv[i]);
        }
    }

    // auto-detect network devices
    DevReaderFactory::findAllDevices();
    const map<string, DevReader*>& devicesDetected = DevReaderFactory::getAllDevReaders();

    map<string, DevReader*> deviceReaders;
    if(devicesRequested.empty() || devicesRequested.front() == "all")
    {
        // use all detected devices
        deviceReaders = devicesDetected;
    }
    else
    {
        // check if requested devices are available
        for(list<string>::const_iterator itRequested = devicesRequested.begin(); itRequested != devicesRequested.end(); ++itRequested)
        {
            map<string, DevReader*>::const_iterator detectedDevice = devicesDetected.find(*itRequested);
            if(detectedDevice != devicesDetected.end())
            {
                deviceReaders[*itRequested] = detectedDevice->second;
            }
            else
            {
                cerr << "no such device: " << *itRequested << endl;
            }
        }
    }

    if(devicesRequested.size() > deviceReaders.size())
    {
        cerr << "some devices not found, aborting" << endl;
        return 0;
    }
    if(deviceReaders.empty())
    {
        cerr << "no devices left, aborting" << endl;
        return 0;
    }

    init();

    // create one instance of the Dev class per device
    unsigned int deviceIndex = 0;
    for(map<string, DevReader*>::const_iterator itDevice = deviceReaders.begin(); itDevice != deviceReaders.end(); ++itDevice)
    {
        Device* device = new Device(*itDevice->second);
        device->setDeviceNumber(deviceIndex++);
        device->setTotalNumberOfDevices(deviceReaders.size());

        device->update();

        m_mainWindow.devices().push_back(device);
    }

    do
    {
        // wait sleep_interval milliseconds (in steps of 100 ms)
        struct timespec wantedTime;
        wantedTime.tv_sec = 0;
        
        int restOfSleepInterval = SettingStore::get("sleep_interval");
        
        while(restOfSleepInterval > 0)
        {
            restOfSleepInterval -= 100;
            wantedTime.tv_nsec = (restOfSleepInterval >= 0 ? 100 : 100 + restOfSleepInterval) * 1000000L;
            
            nanosleep(&wantedTime, 0);
            
            // process keyboard
            int key;
            while((key = getch()) != ERR)
            {
                switch(key)
                {
                    case 'o':
                    case 'O':
                        if(m_optWindow.isVisible())
                        {
                            m_optWindow.hide();
                            m_mainWindow.resize(0, 0, Screen::width(), Screen::height());
                        }
                        else
                        {
                            m_mainWindow.resize(0, Screen::height() / 4, Screen::width(), Screen::height() - Screen::height() / 4);
                            m_optWindow.show(0, 0, Screen::width(), Screen::height() / 4);
                        }
                        restOfSleepInterval = 0; // update the screen
                        break;
                    case 'q':
                    case 'Q':
                        if(!m_optWindow.isVisible())
                            end();
                        break;
                    default:
                        if(m_optWindow.isVisible())
                            m_optWindow.processKey(key);
                        else
                            m_mainWindow.processKey(key);
                        break;
                }
            }
        }
        
        // clear the screen
        m_mainWindow.clear();
        
        // print device data
        m_mainWindow.printTraffic();
        
        // refresh the screen
        m_mainWindow.refresh();
        
        if(m_optWindow.isVisible())
            m_optWindow.refresh(); // always show cursor in option dialog
        
    } while(print_only_once != true); // do this endless except the user said "-t 0"

    end();
    return 0;
}

void init()
{
    // handle interrrupt signal
    signal(SIGINT, end);
    signal(SIGTERM, end);
    signal(SIGWINCH, terminalResized);
    
    // initialize ncurses
    initscr();
    keypad(stdscr, true);
    nodelay(stdscr, true);
    noecho();
    nonl();
    cbreak();
    
    // create main window
    m_mainWindow.show(0, 0, 0, 0);
}

void finish()
{
    // destroy main window
    m_mainWindow.hide();
    
    // stop ncurses
    endwin();
}

void end(int signal)
{
    finish();
    
    vector<Device*>& devices = m_mainWindow.devices();
    for(vector<Device*>::const_iterator i = devices.begin(); i != devices.end(); ++i)
        delete *i;
    devices.clear();
    
    exit(0);
}

void terminalResized(int signal)
{
    bool optWindowWasVisible = m_optWindow.isVisible();

    m_optWindow.hide();

    finish();   
    init();
    
    if(optWindowWasVisible)
    {
        m_mainWindow.resize(0, Screen::height() / 4, Screen::width(), Screen::height() - Screen::height() / 4);
        m_optWindow.show(0, 0, Screen::width(), Screen::height() / 4);
    }
}

void printHelp(bool error)
{
    // print disclaimer
    (error ? cerr : cout)
        << "\n"
        << PACKAGE << " version " << VERSION << "\n"
        << "Copyright (C) 2001 - 2003 by Roland Riegel <feedback@roland-riegel.de>\n"
        << PACKAGE << " comes with ABSOLUTELY NO WARRANTY. This is free software, and you are\n"
        << "welcome to redistribute it under certain conditions. For more details see the\n"
        << "GNU General Public License Version 2 (http://www.gnu.org/copyleft/gpl.html).\n\n"

        << "Command line syntax:\n"
        << PACKAGE << " [options] [devices]\n"
        << PACKAGE << " --help|-h\n\n"

        << "Options:\n"
        << "-a period       Sets the length in seconds of the time window for average\n"
        << "                calculation.\n"
        << "                Default is " << STANDARD_AVERAGE_WINDOW << ".\n"
        << "-i max_scaling  Specifies the 100%% mark in kBit/s of the graph indicating the\n"
        << "                incoming bandwidth usage. Ignored if max_scaling is 0 or the\n"
        << "                switch -m is given.\n"
        << "                Default is " << STANDARD_MAX_DEFLECTION << ".\n"
        << "-m              Show multiple devices at a time; no traffic graphs.\n"
        << "-o max_scaling  Same as -i but for the graph indicating the outgoing bandwidth\n"
        << "                usage.\n"
        << "                Default is " << STANDARD_MAX_DEFLECTION << ".\n"
        << "-t interval     Determines the refresh interval of the display in milliseconds.\n"
        << "                If 0 print net load only once and exit.\n"
        << "                Default is " << STANDARD_SLEEP_INTERVAL << ".\n"
        << "-u h|b|k|m|g    Sets the type of unit used for the display of traffic numbers.\n"
        << "   H|B|K|M|G    h: auto, b: Bit/s, k: kBit/s, m: MBit/s etc.\n"
        << "                H: auto, B: Byte/s, K: kByte/s, M: MByte/s etc.\n"
        << "                Default is k.\n"
        << "-U h|b|k|m|g    Same as -u, but for a total amount of data (without \"/s\").\n"
        << "   H|B|K|M|G    Default is M.\n"
        << "devices         Network devices to use.\n"
        << "                Default is to use all auto-detected devices.\n"
        << "--help\n"
        << "-h              Print this help.\n\n"
        << "example: " << PACKAGE << " -t 200 -s 7 -i 1024 -o 128 -U h eth0 eth1\n\n"
        << "The options above can also be changed at run time by pressing the 'o' key.\n"
        << endl;
}

