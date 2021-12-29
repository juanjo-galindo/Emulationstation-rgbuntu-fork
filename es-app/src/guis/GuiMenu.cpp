#include "guis/GuiMenu.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiSlideshowScreensaverOptions.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>
#include <iostream>
#include "AudioManager.h"
#include "resources/TextureData.h"
#include "animations/LambdaAnimation.h"
#include "guis/GuiThemeInstall.h"
#include "GuiGamelistOptions.h" // grid sizes
#include "platform.h"
#include "renderers/Renderer.h" // setSwapInterval()
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiTextEditPopup.h"
#include "scrapers/ThreadedScraper.h"
#include "ApiSystem.h"
#include "platform.h"
#include "views/gamelist/IGameListView.h"

GuiMenu::GuiMenu(Window* window, bool animate) : GuiComponent(window), mMenu(window, _("MAIN MENU")), mVersion(window)
{
    auto theme = ThemeData::getMenuTheme();

    bool isFullUI = UIModeController::getInstance()->isUIModeFull();    
    
    if (isFullUI)
    {
        addEntry(_("UI SETTINGS"), true, [this] { openUISettings(); }, "iconUI");
        addEntry(_("CONFIGURE INPUT"), true, [this] { openConfigInput(); }, "iconControllers");
    }

    addEntry(_("SOUND SETTINGS"), true, [this] { openSoundSettings(); }, "iconSound");

    if (isFullUI)
    {
        addEntry(_("GAME COLLECTION SETTINGS"), true, [this] { openCollectionSystemSettings(); }, "iconGames");

        // Emulator settings 
        for (auto system : SystemData::sSystemVector)
        {
            if (system->isCollection() || system->getSystemEnvData()->mEmulators.size() == 0 || (system->getSystemEnvData()->mEmulators.size() == 1 && system->getSystemEnvData()->mEmulators[0].mCores.size() <= 1))
                continue;

            addEntry(_("EMULATOR SETTINGS"), true, [this] { openEmulatorSettings(); }, "iconSystem");
            break;
        }
        
        addEntry(_("SCRAPER"), true, [this] { openScraperSettings(); }, "iconScraper");

#if WIN32
        addEntry(_("DOWNLOADS AND UPDATES"), true, [this] { openUpdateSettings(); }, "iconUpdates");
#endif

        addEntry(_("ADVANCED SETTINGS"), true, [this] { openOtherSettings(); }, "iconAdvanced");
    }
    
    addEntry(_("QUIT"), !Settings::getInstance()->getBool("ShowOnlyExit"), [this] {openQuitMenu(); }, "iconQuit");

    addChild(&mMenu);
    addVersionInfo();

    setSize(mMenu.getSize());

    if (animate)    
        animateTo(
            Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.9),
            Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2));
    else
        setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
}

void GuiMenu::openScraperSettings()
{
    auto s = new GuiSettings(mWindow, _("SCRAPER"));

    std::string scraper = Settings::getInstance()->getString("Scraper");

    // scrape from
    auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, _("SCRAPE FROM"), false);
    std::vector<std::string> scrapers = getScraperList();

    // Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
    for (auto it = scrapers.cbegin(); it != scrapers.cend(); it++)
        scraper_list->add(*it, *it, *it == scraper);

    s->addWithLabel(_("SCRAPE FROM"), scraper_list);
    s->addSaveFunc([scraper_list] { Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); });


    if (scraper == "ScreenScraper")
    {
        // Image source : <image> tag
        std::string imageSourceName = Settings::getInstance()->getString("ScrapperImageSrc");
        auto imageSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("IMAGE SOURCE"), false);
        //imageSource->add(_("NONE"), "", imageSourceName.empty());
        imageSource->add(_("SCREENSHOT"), "ss", imageSourceName == "ss");
        imageSource->add(_("TITLE SCREENSHOT"), "sstitle", imageSourceName == "sstitle");
        imageSource->add(_("MIX V1"), "mixrbv1", imageSourceName == "mixrbv1");
        imageSource->add(_("MIX V2"), "mixrbv2", imageSourceName == "mixrbv2");
        imageSource->add(_("BOX 2D"), "box-2D", imageSourceName == "box-2D");
        imageSource->add(_("BOX 3D"), "box-3D", imageSourceName == "box-3D");

        if (!imageSource->hasSelection())
            imageSource->selectFirstItem();

        s->addWithLabel(_("IMAGE SOURCE"), imageSource);
        s->addSaveFunc([imageSource] { Settings::getInstance()->setString("ScrapperImageSrc", imageSource->getSelected()); });

        // Box source : <thumbnail> tag
        std::string thumbSourceName = Settings::getInstance()->getString("ScrapperThumbSrc");
        auto thumbSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOX SOURCE"), false);
        thumbSource->add(_("NONE"), "", thumbSourceName.empty());
        thumbSource->add(_("BOX 2D"), "box-2D", thumbSourceName == "box-2D");
        thumbSource->add(_("BOX 3D"), "box-3D", thumbSourceName == "box-3D");

        if (!thumbSource->hasSelection())
            thumbSource->selectFirstItem();

        s->addWithLabel(_("BOX SOURCE"), thumbSource);
        s->addSaveFunc([thumbSource] { Settings::getInstance()->setString("ScrapperThumbSrc", thumbSource->getSelected()); });

        imageSource->setSelectedChangedCallback([this, thumbSource](std::string value)
        {
            if (value == "box-2D")
                thumbSource->remove(_("BOX 2D"));
            else
                thumbSource->add(_("BOX 2D"), "box-2D", false);

            if (value == "box-3D")
                thumbSource->remove(_("BOX 3D"));
            else
                thumbSource->add(_("BOX 3D"), "box-3D", false);
        });

        // Logo source : <marquee> tag
        std::string logoSourceName = Settings::getInstance()->getString("ScrapperLogoSrc");
        auto logoSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOGO SOURCE"), false);
        logoSource->add(_("NONE"), "", logoSourceName.empty());
        logoSource->add(_("WHEEL"), "wheel", logoSourceName == "wheel");
        logoSource->add(_("MARQUEE"), "marquee", logoSourceName == "marquee");

        if (!logoSource->hasSelection())
            logoSource->selectFirstItem();

        s->addWithLabel(_("LOGO SOURCE"), logoSource);
        s->addSaveFunc([logoSource] { Settings::getInstance()->setString("ScrapperLogoSrc", logoSource->getSelected()); });

        // scrape ratings
        auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
        scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
        s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings);
        s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });

        // scrape video
        auto scrape_video = std::make_shared<SwitchComponent>(mWindow);
        scrape_video->setState(Settings::getInstance()->getBool("ScrapeVideos"));
        s->addWithLabel(_("SCRAPE VIDEOS"), scrape_video);
        s->addSaveFunc([scrape_video] { Settings::getInstance()->setBool("ScrapeVideos", scrape_video->getState()); });

        // Account
        createInputTextRow(s, _("USERNAME"), "ScreenScraperUser", false);
        createInputTextRow(s, _("PASSWORD"), "ScreenScraperPass", true);
    }
    else
    {
        // scrape ratings
        auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
        scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
        s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings); // batocera
        s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });
    }

    // scrape now
    ComponentListRow row;
    auto openScrapeNow = [this] 
    { 
        if (ThreadedScraper::isRunning())
        {
            Window* window = mWindow;

            mWindow->pushGui(new GuiMsgBox(mWindow, _("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT ?"), _("YES"), [this, window]
            {
                ThreadedScraper::stop();
            }, _("NO"), nullptr));

            return;
        }

        mWindow->pushGui(new GuiScraperStart(mWindow)); 
    };
    std::function<void()> openAndSave = openScrapeNow;
    openAndSave = [s, openAndSave] { s->save(); openAndSave(); };
    s->addEntry(_("SCRAPE NOW"), true, openAndSave, "iconScraper");

    s->updatePosition();

    scraper_list->setSelectedChangedCallback([this, s, scraper, scraper_list](std::string value)
    {
        if (value != scraper && (scraper == "ScreenScraper" || value == "ScreenScraper"))
        {
            Settings::getInstance()->setString("Scraper", value);
            delete s;
            openScraperSettings();
        }
    });

    mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings()
{
    Window* window = mWindow;
    auto s = new GuiSettings(mWindow, _("SOUND SETTINGS"));
    
    // volume
    auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
    volume->setValue((float)VolumeControl::getInstance()->getVolume());
    volume->setOnValueChanged([](const float &newVal) { VolumeControl::getInstance()->setVolume((int)Math::round(newVal)); });
    s->addWithLabel(_("SYSTEM VOLUME"), volume);
    //s->addSaveFunc([volume] { VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue())); });

    if (UIModeController::getInstance()->isUIModeFull())
    {

#if defined(__linux__)
        // audio card
        auto audio_card = std::make_shared< OptionListComponent<std::string> >(mWindow, "AUDIO CARD", false);
        std::vector<std::string> audio_cards;
    #ifdef _RPI_
        // RPi Specific  Audio Cards
        audio_cards.push_back("local");
        audio_cards.push_back("hdmi");
        audio_cards.push_back("both");
    #endif
        audio_cards.push_back("default");
        audio_cards.push_back("sysdefault");
        audio_cards.push_back("dmix");
        audio_cards.push_back("hw");
        audio_cards.push_back("plughw");
        audio_cards.push_back("null");
        if (Settings::getInstance()->getString("AudioCard") != "") {
            if(std::find(audio_cards.begin(), audio_cards.end(), Settings::getInstance()->getString("AudioCard")) == audio_cards.end()) {
                audio_cards.push_back(Settings::getInstance()->getString("AudioCard"));
            }
        }
        for(auto ac = audio_cards.cbegin(); ac != audio_cards.cend(); ac++)
            audio_card->add(*ac, *ac, Settings::getInstance()->getString("AudioCard") == *ac);
        s->addWithLabel("AUDIO CARD", audio_card);
        s->addSaveFunc([audio_card] {
            Settings::getInstance()->setString("AudioCard", audio_card->getSelected());
            VolumeControl::getInstance()->deinit();
            VolumeControl::getInstance()->init();
        });

        // volume control device
        auto vol_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, "AUDIO DEVICE", false);
        std::vector<std::string> transitions;
        transitions.push_back("PCM");
        transitions.push_back("Speaker");
        transitions.push_back("Master");
        transitions.push_back("Digital");
        transitions.push_back("Analogue");
        if (Settings::getInstance()->getString("AudioDevice") != "") {
            if(std::find(transitions.begin(), transitions.end(), Settings::getInstance()->getString("AudioDevice")) == transitions.end()) {
                transitions.push_back(Settings::getInstance()->getString("AudioDevice"));
            }
        }
        for(auto it = transitions.cbegin(); it != transitions.cend(); it++)
            vol_dev->add(*it, *it, Settings::getInstance()->getString("AudioDevice") == *it);
        s->addWithLabel("AUDIO DEVICE", vol_dev);
        s->addSaveFunc([vol_dev] {
            Settings::getInstance()->setString("AudioDevice", vol_dev->getSelected());
            VolumeControl::getInstance()->deinit();
            VolumeControl::getInstance()->init();
        });
#endif
        // disable sounds
        auto music_enabled = std::make_shared<SwitchComponent>(mWindow);
        music_enabled->setState(Settings::getInstance()->getBool("audio.bgmusic"));
        s->addWithLabel(_("FRONTEND MUSIC"), music_enabled);
        s->addSaveFunc([music_enabled] {
            Settings::getInstance()->setBool("audio.bgmusic", music_enabled->getState());
            if (music_enabled->getState()){
                //AudioManager::getInstance()->playRandomMusic(false,true); 
                ViewController::get()->ReloadSoundSystem(true);
            } else {
                AudioManager::getInstance()->stopMusic();
            }
        });
                               
        // rgbuntu - music per system
        auto musicPerSystem = std::make_shared<SwitchComponent>(mWindow);
        //enableThemeMusics->setState(Settings::getInstance()->getBool("audio.thememusics"));
        musicPerSystem->setState(Settings::getInstance()->getBool("audio.persystem"));
        //s->addWithLabel(_("PLAY THEME MUSICS"), enableThemeMusics);
        s->addWithLabel(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), musicPerSystem);
        s->addSaveFunc([musicPerSystem] {
            //if (Settings::getInstance()->setBool("audio.thememusics", enableThemeMusics->getState()))
            if (Settings::getInstance()->setBool("audio.persystem", musicPerSystem->getState())){
                //AudioManager::getInstance()->playRandomMusic(false,true);
                ViewController::get()->ReloadSoundSystem(true);
            }
        });

        //auto s = new GuiSettings(mWindow, _("SOUND SETTINGS"));
        //GuiSettings* configuration = new GuiSettings(mWindow, _("PLAYLIST AUDIO SETTINGS").c_str());  
        s->addEntry(_("MUSIC FOLDER SETTINGS"), true, [this] { openSystemSpecificMusicFolderSettings(); }, "iconSystem");

        //display music info
        auto display_song_info = std::make_shared<SwitchComponent>(mWindow);
        display_song_info->setState(Settings::getInstance()->getBool("MusicInfo"));
        s->addWithLabel(_("DISPLAY MUSIC INFO"), display_song_info);
        s->addSaveFunc([display_song_info] {
            Settings::getInstance()->setBool("MusicInfo", display_song_info->getState());
        });

        // rgbuntu - music info selection 
        auto song_info_selection= std::make_shared< OptionListComponent<std::string> >(mWindow, _("MUSIC INFO"), false);
        std::vector<std::string> songInfoModes;
        songInfoModes.push_back("Title");
        songInfoModes.push_back("Path");
        //songInfoModes.push_back("TEST");
         
        for (auto it = songInfoModes.cbegin(); it != songInfoModes.cend(); it++)
            song_info_selection->add(_(*it), *it, Settings::getInstance()->getString("MusicInfoModes") == *it);

        if (!song_info_selection->hasSelection())
            song_info_selection->selectFirstItem();

        s->addWithLabel(_("MUSIC INFO"), song_info_selection);
        s->addSaveFunc([song_info_selection] {
            Settings::getInstance()->setString("MusicInfoModes", song_info_selection->getSelected());
            //AudioManager::getInstance()->themeChanged(ViewController::get()->getState().getSystem()->getTheme(), true);
        });


        // RGBuntu - random music list
        auto random_music_list = std::make_shared<SwitchComponent>(mWindow);
        random_music_list->setState(Settings::getInstance()->getBool("RandomMusicList"));
        s->addWithLabel(_("RANDOM PLAYLIST"), random_music_list);
        s->addSaveFunc([random_music_list] {
            Settings::getInstance()->setBool("RandomMusicList", random_music_list->getState());
        });

        //rgbuntu - enable track list controls
        auto enable_playback_control = std::make_shared<SwitchComponent>(mWindow);
        enable_playback_control->setState(Settings::getInstance()->getBool("EnablePlaybackControl"));
        s->addWithLabel(_("ENABLE PLAYBACK CONTROL"), enable_playback_control);
        s->addSaveFunc([enable_playback_control, window] {
            Settings::getInstance()->setBool("EnablePlaybackControl", enable_playback_control->getState());
            bool enabled = Settings::getInstance()->getBool("EnablePlaybackControl");
            bool playbackControlNotified = Settings::getInstance()->getBool("PlaybackControlNotified");
            if (enabled && !playbackControlNotified) {
                Settings::getInstance()->setBool("PlaybackControlNotified", true);
                std::string msg = "";
                if (Settings::getInstance()->getString("Language")=="es" ||
                    Settings::getInstance()->getString("Language")=="Es" || 
                    Settings::getInstance()->getString("Language")=="ES" || 
                    Settings::getInstance()->getString("Language")=="español" || 
                    Settings::getInstance()->getString("Language")=="Español" || 
                    Settings::getInstance()->getString("Language")=="ESPAÑOL" || 
                    Settings::getInstance()->getString("Language")=="castellano" || 
                    Settings::getInstance()->getString("Language")=="Castellano" || 
                    Settings::getInstance()->getString("Language")=="CASTELLANO" 
                   ) {
                    msg += " Los controles de reproducción están habilitados en la Vista de Sistema!!\n";
                    msg += " \n";
                    msg += " Arriba : Reproduce el archivo SIGUIENTE.\n";
                    msg += " Abajo  : Reproduce el archivo ANTERIOR.\n";
                    msg += " \n";
                    msg += " Excepto con el Carrusel Vertical:\n";
                    msg += " Derecha  : Reproduce el archivo SIGUIENTE.\n";
                    msg += " Izquierda: Reproduce el archivo ANTERIOR.\n";
                } else if (Settings::getInstance()->getString("Language")=="ca" ||
                           Settings::getInstance()->getString("Language")=="CA" || 
                           Settings::getInstance()->getString("Language")=="catala" || 
                           Settings::getInstance()->getString("Language")=="català" || 
                           Settings::getInstance()->getString("Language")=="Catala" || 
                           Settings::getInstance()->getString("Language")=="Català" || 
                           Settings::getInstance()->getString("Language")=="CATALA" || 
                           Settings::getInstance()->getString("Language")=="CATALÀ"
                          ) {
                    msg += " Els controls de reproducció estan habilitats a la Vista de Sistema!\n";
                    msg += " \n";
                    msg += " Adalt: Reprodueix l'arxiu SEGÜENT.\n";
                    msg += " Abaix: Reprodueix l'arxiu ANTERIOR.\n";
                    msg += " \n";
                    msg += " Excepte amb el Carrusell Vertical:\n";
                    msg += " Dreta: Reprodueix l'arxiu SEGÜENT.\n";
                    msg += " Esquerra: Reprodueix l'arxiu ANTERIOR.\n";
                } else {
                    msg += " Playback control is enabled in SystemView !!\n";
                    msg += " \n";
                    msg += " Up : Plays NEXT track.\n";
                    msg += " Down : Plays PREVIOUS track.\n";
                    msg += " \n";
                    msg += " Though when using Vertical Carousel:\n";
                    msg += " Right: Plays NEXT track.\n";
                    msg += " Left: Plays PREVIOUS track.\n";
                }
                window->pushGui(new GuiMsgBox(window, _(msg), _("OK"))); 
            }
            if (!enabled) {
                Settings::getInstance()->setBool("PlaybackControlNotified", false);
            }
        });

        //// music per system (rgbuntu - removed)
        //auto music_per_system = std::make_shared<SwitchComponent>(mWindow);
        //music_per_system->setState(Settings::getInstance()->getBool("audio.persystem"));
        //s->addWithLabel(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), music_per_system);
        //s->addSaveFunc([music_per_system] {
        //  Settings::getInstance()->setBool("audio.persystem", music_per_system->getState());
        //});

        // disable sounds
        auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
        sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
        s->addWithLabel(_("ENABLE NAVIGATION SOUNDS"), sounds_enabled);
        s->addSaveFunc([sounds_enabled] {
            if (sounds_enabled->getState()
                && !Settings::getInstance()->getBool("EnableSounds")
                && PowerSaver::getMode() == PowerSaver::INSTANT)
            {
                Settings::getInstance()->setString("PowerSaverMode", "default");
                PowerSaver::init();
            }
            Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState());
        });

        auto video_audio = std::make_shared<SwitchComponent>(mWindow);
        video_audio->setState(Settings::getInstance()->getBool("VideoAudio"));
        s->addWithLabel(_("ENABLE VIDEO AUDIO"), video_audio);
        s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });

        auto videolowermusic = std::make_shared<SwitchComponent>(mWindow);
        videolowermusic->setState(Settings::getInstance()->getBool("VideoLowersMusic"));
        s->addWithLabel(_("LOWER MUSIC WHEN PLAYING VIDEO"), videolowermusic);
        s->addSaveFunc([videolowermusic] { Settings::getInstance()->setBool("VideoLowersMusic", videolowermusic->getState()); });

#ifdef _RPI_
        // OMX player Audio Device
        auto omx_audio_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, "OMX PLAYER AUDIO DEVICE", false);
        std::vector<std::string> omx_cards;
        // RPi Specific  Audio Cards
        omx_cards.push_back("local");
        omx_cards.push_back("hdmi");
        omx_cards.push_back("both");
        omx_cards.push_back("alsa:hw:0,0");
        omx_cards.push_back("alsa:hw:1,0");
        if (Settings::getInstance()->getString("OMXAudioDev") != "") {
            if (std::find(omx_cards.begin(), omx_cards.end(), Settings::getInstance()->getString("OMXAudioDev")) == omx_cards.end()) {
                omx_cards.push_back(Settings::getInstance()->getString("OMXAudioDev"));
            }
        }
        for (auto it = omx_cards.cbegin(); it != omx_cards.cend(); it++)
            omx_audio_dev->add(*it, *it, Settings::getInstance()->getString("OMXAudioDev") == *it);
        s->addWithLabel("OMX PLAYER AUDIO DEVICE", omx_audio_dev);
        s->addSaveFunc([omx_audio_dev] {
            if (Settings::getInstance()->getString("OMXAudioDev") != omx_audio_dev->getSelected())
                Settings::getInstance()->setString("OMXAudioDev", omx_audio_dev->getSelected());
        });
#endif
    }

    s->updatePosition();
    mWindow->pushGui(s);

}

struct ThemeConfigOption
{
    std::string defaultSettingName;
    std::string subset;
    std::shared_ptr<OptionListComponent<std::string>> component;
};

void GuiMenu::openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set,const std::string systemTheme)
{
    if (theme_set != nullptr && Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
    {
        mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU MUST APPLY THE THEME BEFORE EDIT CONFIGURATION"), _("OK")));
        return;
    }

    Window* window = mWindow;

    auto system = ViewController::get()->getState().getSystem();
    auto theme = system->getTheme();

    auto themeconfig = new GuiSettings(mWindow, (systemTheme.empty() ? _("THEME CONFIGURATION") : _("VIEW CUSTOMISATION")).c_str());

    auto themeSubSets = theme->getSubSets();

    std::string viewName;
    bool showGridFeatures = true;
    if (!systemTheme.empty())
    {
        auto glv = ViewController::get()->getGameListView(system);
        viewName = glv->getName();
        std::string baseType = theme->getCustomViewBaseType(viewName);

        showGridFeatures = (viewName == "grid" || baseType == "grid");
    }


    // gamelist_style
    std::shared_ptr<OptionListComponent<std::string>> gamelist_style = nullptr;

    if (systemTheme.empty())
    {
        gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);

        std::vector<std::pair<std::string, std::string>> styles;
        styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

        if (system != NULL)
        {
            auto mViews = theme->getViewsOfTheme();
            for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
            {
                if (it->first == "basic" || it->first == "detailed" || it->first == "grid")
                    styles.push_back(std::pair<std::string, std::string>(it->first, _(it->first.c_str())));
                else
                    styles.push_back(*it);
            }
        }
        else
        {
            styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
            styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
        }

        auto viewPreference = systemTheme.empty() ? Settings::getInstance()->getString("GamelistViewStyle") : system->getSystemViewMode();
        if (!theme->hasView(viewPreference))
            viewPreference = "automatic";

        for (auto it = styles.cbegin(); it != styles.cend(); it++)
            gamelist_style->add(it->second, it->first, viewPreference == it->first);

        if (!gamelist_style->hasSelection())
            gamelist_style->selectFirstItem();

        themeconfig->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
    }

    // Default grid size
    std::shared_ptr<OptionListComponent<std::string>> mGridSize = nullptr;
    if (showGridFeatures && system != NULL && theme->hasView("grid"))
    {
        Vector2f gridOverride =
            systemTheme.empty() ? Vector2f::parseString(Settings::getInstance()->getString("DefaultGridSize")) :
            system->getGridSizeOverride();

        auto ovv = std::to_string((int)gridOverride.x()) + "x" + std::to_string((int)gridOverride.y());

        mGridSize = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DEFAULT GRID SIZE"), false);

        bool found = false;
        for (auto it = GuiGamelistOptions::gridSizes.cbegin(); it != GuiGamelistOptions::gridSizes.cend(); it++)
        {
            bool sel = (gridOverride == Vector2f(0, 0) && *it == "automatic") || ovv == *it;
            if (sel)
                found = true;

            mGridSize->add(_(it->c_str()), *it, sel);
        }

        if (!found)
            mGridSize->selectFirstItem();

        themeconfig->addWithLabel(_("DEFAULT GRID SIZE"), mGridSize);
    }

    std::map<std::string, ThemeConfigOption> options;

    for (std::string subset : theme->getSubSetNames(viewName))
    {
        std::string settingName = "subset." + subset;
        std::string perSystemSettingName = systemTheme.empty() ? "" : "subset." + systemTheme + "." + subset;

        if (subset == "colorset") settingName = "ThemeColorSet";
        else if (subset == "iconset") settingName = "ThemeIconSet";
        else if (subset == "menu") settingName = "ThemeMenu";
        else if (subset == "systemview") settingName = "ThemeSystemView";
        else if (subset == "gamelistview") settingName = "ThemeGamelistView";
        else if (subset == "region") settingName = "ThemeRegionName";

        auto themeColorSets = ThemeData::getSubSet(themeSubSets, subset);
            
        if (themeColorSets.size() > 0)
        {
            auto selectedColorSet = themeColorSets.end();
            auto selectedName = !perSystemSettingName.empty() ? Settings::getInstance()->getString(perSystemSettingName) : Settings::getInstance()->getString(settingName);

            if (!perSystemSettingName.empty() && selectedName.empty())
                selectedName = Settings::getInstance()->getString(settingName);

            for (auto it = themeColorSets.begin(); it != themeColorSets.end() && selectedColorSet == themeColorSets.end(); it++)
                if (it->name == selectedName)
                    selectedColorSet = it;

            std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, _(("THEME " + Utils::String::toUpper(subset)).c_str()), false);
            item->setTag(!perSystemSettingName.empty()? perSystemSettingName : settingName);

            for (auto it = themeColorSets.begin(); it != themeColorSets.end(); it++)
            {
                std::string displayName = it->displayName;

                if (!systemTheme.empty())
                {
                    std::string defaultValue = Settings::getInstance()->getString(settingName);
                    if (defaultValue.empty())
                        defaultValue = system->getTheme()->getDefaultSubSetValue(subset);

                    if (it->name == defaultValue)
                        displayName = displayName + " (" + _("DEFAULT") +")";
                }

                item->add(displayName, it->name, it == selectedColorSet);
            }

            if (selectedColorSet == themeColorSets.end())
                item->selectFirstItem();

            if (!themeColorSets.empty())
            {                       
                std::string displayName = themeColorSets.cbegin()->subSetDisplayName;
                if (!displayName.empty())
                {
                    std::string prefix;

                    if (systemTheme.empty())
                    {
                        for (auto subsetName : themeColorSets.cbegin()->appliesTo)
                        {
                            std::string pfx = theme->getViewDisplayName(subsetName);
                            if (!pfx.empty())
                            {
                                if (prefix.empty())
                                    prefix = pfx;
                                else
                                    prefix = prefix + ", " + pfx;                               
                            }
                        }

                        if (!prefix.empty())
                            prefix = " ("+ prefix+")";

                    }                   

                    themeconfig->addWithLabel(displayName + prefix, item);
                }
                else
                    themeconfig->addWithLabel(_(("THEME " + Utils::String::toUpper(subset)).c_str()), item);
            }

            ThemeConfigOption opt;
            opt.component = item;
            opt.subset = subset;            
            opt.defaultSettingName = settingName;
            options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
        }
        else
        {
            ThemeConfigOption opt;
            opt.component = nullptr;
            options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
        }
    }


    if (systemTheme.empty())
    {
        themeconfig->addEntry(_("RESET GAMELIST CUSTOMISATIONS"), false, [s, themeconfig, window]
        {
            Settings::getInstance()->setString("GamelistViewStyle", "");
            Settings::getInstance()->setString("DefaultGridSize", "");

            for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
                (*sysIt)->setSystemViewMode("automatic", Vector2f(0, 0));

            themeconfig->setVariable("reloadAll", true);
            themeconfig->close();
        });
    }

    //  theme_colorset, theme_iconset, theme_menu, theme_systemview, theme_gamelistview, theme_region,
    themeconfig->addSaveFunc([systemTheme, system, themeconfig, theme_set, options, gamelist_style, mGridSize, window]
    {
        bool reloadAll = systemTheme.empty() ? Settings::getInstance()->setString("ThemeSet", theme_set == nullptr ? "" : theme_set->getSelected()) : false;

        for (auto option : options)
        {
            ThemeConfigOption& opt = option.second;

            std::string value;

            if (opt.component != nullptr)
            {
                value = opt.component->getSelected();

                if (!systemTheme.empty() && !value.empty())
                {
                    std::string defaultValue = Settings::getInstance()->getString(opt.defaultSettingName);
                    if (defaultValue.empty())
                        defaultValue = system->getTheme()->getDefaultSubSetValue(opt.subset);

                    if (value == defaultValue)
                        value = "";
                }
                else if (systemTheme.empty() && value == system->getTheme()->getDefaultSubSetValue(opt.subset))
                    value = "";
            }

            if (value != Settings::getInstance()->getString(option.first))
                reloadAll |= Settings::getInstance()->setString(option.first, value);
        }
    
        Vector2f gridSizeOverride(0, 0);

        if (mGridSize != nullptr)
        {
            std::string str = mGridSize->getSelected();
            std::string value = "";

            size_t divider = str.find('x');
            if (divider != std::string::npos)
            {
                std::string first = str.substr(0, divider);
                std::string second = str.substr(divider + 1, std::string::npos);

                gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
                value = Utils::String::replace(Utils::String::replace(gridSizeOverride.toString(), ".000000", ""), "0 0", "");
            }

            if (systemTheme.empty())
                reloadAll |= Settings::getInstance()->setString("DefaultGridSize", value);
        }
        else if (systemTheme.empty())
            reloadAll |= Settings::getInstance()->setString("DefaultGridSize", "");
        
        if (systemTheme.empty())
            reloadAll |= Settings::getInstance()->setString("GamelistViewStyle", gamelist_style == nullptr ? "" : gamelist_style->getSelected());
        else
        {
            std::string viewMode = gamelist_style == nullptr ? system->getSystemViewMode() : gamelist_style->getSelected();
            reloadAll |= system->setSystemViewMode(viewMode, gridSizeOverride);
        }

        if (reloadAll || themeconfig->getVariable("reloadAll"))
        {   
            if (systemTheme.empty())
            {
                CollectionSystemManager::get()->updateSystemsList();
                ViewController::get()->reloadAll(window);
                window->endRenderLoadingScreen();

                if (theme_set != nullptr)
                {
                    std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
                    Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);
                }
            }
            else
            {
                system->loadTheme();
                system->resetFilters();
                ViewController::get()->reloadGameListView(system);
            }
        }
    });

    mWindow->pushGui(themeconfig);
}

std::string GuiMenu::runCommand(const char * command)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    std::cout << result << std::endl;
    return result;
}
                   
void GuiMenu::openUISettings()
{   
    auto pthis = this;
    Window* window = mWindow;

    auto s = new GuiSettings(mWindow, _("UI SETTINGS"));

    std::string msg_check = "";
    msg_check = runCommand("ls /dev/disk/by-uuid/ |grep 39a84b9f9714");
    std::string msg = "";
    msg +=" Esta versión de ES ha sido ";
    msg +=" desarrollada por : https://t.me/Juanjo_Galindo";
    msg +=" para RGBuntu, a partir del fork de Fabrice Caruso.";
    msg +="  ";
    msg +=" Si estas leyendo esto, deberias probar RGBuntu ;-)   ";
    msg +="  ";
    msg +=" *AGRADECIMIENTOS:";
    msg +=" al equipo de Desarrollo de RGBuntu; ";
    msg +=" a la comunidad de usuarios; ";
    msg +=" y a Fabrice Caruso: https://github.com/fabricecaruso/";
    //if (msg_check==std::string("9fc5f5a9-ee53-47cb-adad-39a84b9f9714"))
    if (msg_check==std::string(""))
        window->pushGui(new GuiMsgBox(window, _(msg), _("OK"))); 
    msg_check = runCommand("clear");
    
    // theme set
    auto theme = ThemeData::getMenuTheme();
    auto themeSets = ThemeData::getThemeSets();
    auto system = ViewController::get()->getState().getSystem();

    if (!themeSets.empty())
    {
        std::map<std::string, ThemeSet>::const_iterator selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
        if (selectedSet == themeSets.cend())
            selectedSet = themeSets.cbegin();

        auto theme_set = std::make_shared< OptionListComponent<std::string> >(mWindow, "THEME", false);
        for (auto it = themeSets.cbegin(); it != themeSets.cend(); it++)
            theme_set->add(it->first, it->first, it == selectedSet);

        s->addWithLabel(_("THEME"), theme_set);
        s->addSaveFunc([s, theme_set, window]
        {
            std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
            if (oldTheme != theme_set->getSelected())
            {
                Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

                // theme changed without setting options, forcing options to avoid crash/blank theme
                Settings::getInstance()->setString("ThemeRegionName", "");
                Settings::getInstance()->setString("ThemeColorSet", "");
                Settings::getInstance()->setString("ThemeIconSet", "");
                Settings::getInstance()->setString("ThemeMenu", "");
                Settings::getInstance()->setString("ThemeSystemView", "");
                Settings::getInstance()->setString("ThemeGamelistView", "");
                Settings::getInstance()->setString("GamelistViewStyle", "");
                Settings::getInstance()->setString("DefaultGridSize", "");

                for(auto sm : Settings::getInstance()->getStringMap())
                    if (Utils::String::startsWith(sm.first, "subset."))
                        Settings::getInstance()->setString(sm.first, "");

                for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
                    (*sysIt)->setSystemViewMode("automatic", Vector2f(0, 0));

                s->setVariable("reloadCollections", true);
                s->setVariable("reloadAll", true);
                s->setVariable("reloadGuiMenu", true);

                Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);
            }
        });
    
        bool showThemeConfiguration = system->getTheme()->hasSubsets() || system->getTheme()->hasView("grid");
        if (showThemeConfiguration)
        {
            s->addSubMenu(_("THEME CONFIGURATION"), [this, s, theme_set]() { openThemeConfiguration(mWindow, s, theme_set); });
        }
        else // GameList view style only, acts like Retropie for simple themes
        {
            auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
            std::vector<std::pair<std::string, std::string>> styles;
            styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

            auto system = ViewController::get()->getState().getSystem();
            if (system != NULL)
            {
                auto mViews = system->getTheme()->getViewsOfTheme();
                for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
                    styles.push_back(*it);
            }
            else
            {
                styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
                styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
                styles.push_back(std::pair<std::string, std::string>("video", _("video")));
                styles.push_back(std::pair<std::string, std::string>("grid", _("grid")));
            }

            auto viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
            if (!system->getTheme()->hasView(viewPreference))
                viewPreference = "automatic";

            for (auto it = styles.cbegin(); it != styles.cend(); it++)
                gamelist_style->add(it->second, it->first, viewPreference == it->first);

            s->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
            s->addSaveFunc([s, gamelist_style, window] 
            {
                if (Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected()))
                {
                    s->setVariable("reloadAll", true);
                    s->setVariable("reloadGuiMenu", true);
                }
            });
        }
    }

    // screensaver
    ComponentListRow screensaver_row;
    screensaver_row.elements.clear();
    screensaver_row.addElement(std::make_shared<TextComponent>(mWindow, _("SCREENSAVER SETTINGS"), theme->Text.font, theme->Text.color), true);
    screensaver_row.addElement(makeArrow(mWindow), false);
    screensaver_row.makeAcceptInputHandler(std::bind(&GuiMenu::openScreensaverOptions, this));
    s->addRow(screensaver_row);


    //#ifndef WIN32
        //UI mode
    auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("UI MODE"), false);
    std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
    for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
        UImodeSelection->add(_(*it), *it, Settings::getInstance()->getString("UIMode") == *it);
    s->addWithLabel(_("UI MODE"), UImodeSelection);

    s->addSaveFunc([UImodeSelection, window]
    {
        std::string selectedMode = UImodeSelection->getSelected();
        if (selectedMode != "Full")
        {
            std::string msg = "You are changing the UI to a restricted mode:\n" + selectedMode + "\n";
            msg += "This will hide most menu-options to prevent changes to the system.\n";
            msg += "To unlock and return to the full UI, enter this code: \n";
            msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
            msg += "Do you want to proceed?";
            window->pushGui(new GuiMsgBox(window, msg,
                "YES", [selectedMode] {
                LOG(LogDebug) << "Setting UI mode to " << selectedMode;
                Settings::getInstance()->setString("UIMode", selectedMode);
                Settings::getInstance()->saveFile();
            }, "NO", nullptr));
        }
    });
    //#endif

    // LANGUAGE
    /*
    std::vector<std::string> langues;
    langues.push_back("en");

    std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/splash.svg");
    if (xmlpath.length() > 0)
    {
        xmlpath = Utils::FileSystem::getParent(xmlpath) + "/locale/";

        Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(xmlpath, true);
        for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
        {
            if (Utils::FileSystem::isDirectory(*it))
                continue;

            std::string name = *it;

            if (name.rfind("emulationstation2.po") == std::string::npos)
                continue;

            name = Utils::FileSystem::getParent(name);
            name = Utils::FileSystem::getFileName(name);

            if (name != "en")
                langues.push_back(name);
        }

        if (langues.size() > 1)
        {
            auto language = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LANGUAGE"), false);

            for (auto it = langues.cbegin(); it != langues.cend(); it++)
                language->add(*it, *it, Settings::getInstance()->getString("Language") == *it);

            s->addWithLabel(_("LANGUAGE"), language);
            s->addSaveFunc([language, window, pthis, s] {
                
                if (language->getSelected() != Settings::getInstance()->getString("Language"))
                {
                    if (Settings::getInstance()->setString("Language", language->getSelected()))
                        s->setVariable("reloadGuiMenu", true);
                }
            });
        }
    }
    */
    // transition style
    auto transition_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("TRANSITION STYLE"), false);
    std::vector<std::string> transitions;
    transitions.push_back("auto");
    transitions.push_back("fade");
    transitions.push_back("slide");
    transitions.push_back("instant");
    
    for (auto it = transitions.cbegin(); it != transitions.cend(); it++)
        transition_style->add(_(*it), *it, Settings::getInstance()->getString("TransitionStyle") == *it);

    if (!transition_style->hasSelection())
        transition_style->selectFirstItem();

    s->addWithLabel(_("TRANSITION STYLE"), transition_style);
    s->addSaveFunc([transition_style] {
        if (Settings::getInstance()->getString("TransitionStyle") == "instant"
            && transition_style->getSelected() != "instant"
            && PowerSaver::getMode() == PowerSaver::INSTANT)
        {
            Settings::getInstance()->setString("PowerSaverMode", "default");
            PowerSaver::init();
        }
        Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
        GuiComponent::ALLOWANIMATIONS = Settings::getInstance()->getString("TransitionStyle") != "instant";
    });
    
    auto transitionOfGames_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAME LAUNCH TRANSITION"), false);
    std::vector<std::string> gameTransitions;
    gameTransitions.push_back("fade");
    gameTransitions.push_back("slide");
    gameTransitions.push_back("instant");
    for (auto it = gameTransitions.cbegin(); it != gameTransitions.cend(); it++)
        transitionOfGames_style->add(_(*it), *it, Settings::getInstance()->getString("GameTransitionStyle") == *it);

    s->addWithLabel(_("GAME LAUNCH TRANSITION"), transitionOfGames_style);
    s->addSaveFunc([transitionOfGames_style] {
        if (Settings::getInstance()->getString("GameTransitionStyle") == "instant"
            && transitionOfGames_style->getSelected() != "instant"
            && PowerSaver::getMode() == PowerSaver::INSTANT)
        {
            Settings::getInstance()->setString("PowerSaverMode", "default");
            PowerSaver::init();
        }
        Settings::getInstance()->setString("GameTransitionStyle", transitionOfGames_style->getSelected());
    });


    // Optionally start in selected system
    auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, _("START ON SYSTEM"), false);
    systemfocus_list->add(_("NONE"), "", Settings::getInstance()->getString("StartupSystem") == "");

    for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
        if ("retropie" != (*it)->getName() && (*it)->isVisible())
            systemfocus_list->add((*it)->getName(), (*it)->getName(), Settings::getInstance()->getString("StartupSystem") == (*it)->getName());

    if (!systemfocus_list->hasSelection())
        systemfocus_list->selectFirstItem();

    s->addWithLabel(_("START ON SYSTEM"), systemfocus_list);
    s->addSaveFunc([systemfocus_list] {
        Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected());
    });



    // Select systems to hide
    auto hiddenSystems = Utils::String::split(Settings::getInstance()->getString("HiddenSystems"), ';');

    auto displayedSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("VISIBLE SYSTEMS"), true);

    for (auto system : SystemData::sSystemVector)
        if(!system->isCollection() && !system->isGroupChildSystem())
            displayedSystems->add(system->getFullName(), system, std::find(hiddenSystems.cbegin(), hiddenSystems.cend(), system->getName()) == hiddenSystems.cend());

    s->addWithLabel(_("VISIBLE SYSTEMS"), displayedSystems);
    s->addSaveFunc([s, displayedSystems]
    {
        std::string hiddenSystems;

        std::vector<SystemData*> sys = displayedSystems->getSelectedObjects();

        for (auto system : SystemData::sSystemVector)
        {
            if (system->isCollection() || system->isGroupChildSystem())
                continue;

            if (std::find(sys.cbegin(), sys.cend(), system) == sys.cend())
            {
                if (hiddenSystems.empty())
                    hiddenSystems = system->getName();
                else
                    hiddenSystems = hiddenSystems + ";" + system->getName();
            }
        }

        if (Settings::getInstance()->setString("HiddenSystems", hiddenSystems))
        {
            Settings::getInstance()->saveFile();
            s->setVariable("reloadAll", true);
        }       
    });

    
    // Open gamelist at start
    auto bootOnGamelist = std::make_shared<SwitchComponent>(mWindow);
    bootOnGamelist->setState(Settings::getInstance()->getBool("StartupOnGameList"));
    s->addWithLabel(_("BOOT ON GAMELIST"), bootOnGamelist);
    s->addSaveFunc([bootOnGamelist] { Settings::getInstance()->setBool("StartupOnGameList", bootOnGamelist->getState()); });

    // Hide system view
    auto hideSystemView = std::make_shared<SwitchComponent>(mWindow);
    hideSystemView->setState(Settings::getInstance()->getBool("HideSystemView"));
    s->addWithLabel(_("HIDE SYSTEM VIEW"), hideSystemView);
    s->addSaveFunc([hideSystemView] 
    { 
        bool hideSysView = Settings::getInstance()->getBool("HideSystemView");
        Settings::getInstance()->setBool("HideSystemView", hideSystemView->getState());

        if (!hideSysView && hideSystemView->getState())
            ViewController::get()->goToStart(true);
    });


#if defined(_WIN32)
    // quick system select (left/right in game list view)
    auto hideWindowScreen = std::make_shared<SwitchComponent>(mWindow);
    hideWindowScreen->setState(Settings::getInstance()->getBool("HideWindow"));
    s->addWithLabel(_("HIDE WHEN RUNNING GAME"), hideWindowScreen);
    s->addSaveFunc([hideWindowScreen] { Settings::getInstance()->setBool("HideWindow", hideWindowScreen->getState()); });
#endif

    // quick system select (left/right in game list view)
    auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
    quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
    s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
    s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

    // carousel transition option
    auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
    move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
    s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
    s->addSaveFunc([move_carousel] {
        if (move_carousel->getState()
            && !Settings::getInstance()->getBool("MoveCarousel")
            && PowerSaver::getMode() == PowerSaver::INSTANT)
        {
            Settings::getInstance()->setString("PowerSaverMode", "default");
            PowerSaver::init();
        }
        Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState());
    });

    // clock
    auto clock = std::make_shared<SwitchComponent>(mWindow);
    clock->setState(Settings::getInstance()->getBool("DrawClock"));
    s->addWithLabel(_("SHOW CLOCK"), clock);
    s->addSaveFunc(
        [clock] { Settings::getInstance()->setBool("DrawClock", clock->getState()); });

    // show help
    auto show_help = std::make_shared<SwitchComponent>(mWindow);
    show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
    s->addWithLabel(_("ON-SCREEN HELP"), show_help);
    s->addSaveFunc([s, show_help]
    {
        if (Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()))
            s->setVariable("reloadAll", true);
    });

    // filenames
    auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
    hidden_files->setState(Settings::getInstance()->getBool("ShowFilenames"));
    s->addWithLabel(_("SHOW FILENAMES IN LISTS"), hidden_files);
    s->addSaveFunc([hidden_files, s] 
    { 
        if (Settings::getInstance()->setBool("ShowFilenames", hidden_files->getState()))
        {
            FileData::resetSettings();
            s->setVariable("reloadCollections", true);
            s->setVariable("reloadAll", true);
        }
    });

    // enable filters (ForceDisableFilters)
    auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
    enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
    s->addWithLabel(_("ENABLE FILTERS"), enable_filter);
    s->addSaveFunc([enable_filter, s] { 
        bool filter_is_enabled = !Settings::getInstance()->getBool("ForceDisableFilters");
        if (Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState()))
            s->setVariable("reloadAll", true);      
    });


    s->onFinalize([s, pthis, window]
    {
        if (s->getVariable("reloadCollections"))
            CollectionSystemManager::get()->updateSystemsList();

        if (s->getVariable("reloadAll"))
        {
            ViewController::get()->reloadAll(window);
            window->endRenderLoadingScreen();
        }

        if (s->getVariable("reloadGuiMenu"))
        {
            delete pthis;
            window->pushGui(new GuiMenu(window, false));
        }
    });

    s->updatePosition();
    mWindow->pushGui(s);
}

void GuiMenu::openSystemEmulatorSettings(SystemData* system)
{
    auto theme = ThemeData::getMenuTheme();

    GuiSettings* s = new GuiSettings(mWindow, system->getFullName().c_str());

    auto emul_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATOR"), false);
    auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CORE"), false);

    std::string currentEmul = Settings::getInstance()->getString(system->getName() + ".emulator");
    std::string defaultEmul = (system->getSystemEnvData()->mEmulators.size() == 0 ? "" : system->getSystemEnvData()->mEmulators[0].mName);

//  if (defaultEmul.length() == 0)
        emul_choice->add(_("AUTO"), "", false);
//  else
//      emul_choice->add(_("AUTO") + " (" + defaultEmul + ")", "", currentEmul.length() == 0);

    bool found = false;
    for (auto core : system->getSystemEnvData()->mEmulators)
    {
        if (core.mName == currentEmul)
            found = true;

        emul_choice->add(core.mName, core.mName, core.mName == currentEmul);
    }

    if (!found)
        emul_choice->selectFirstItem();
    
    ComponentListRow row;
    row.addElement(std::make_shared<TextComponent>(mWindow, _("EMULATOR"), theme->Text.font, theme->Text.color), true);
    row.addElement(emul_choice, false);

    s->addRow(row);

    emul_choice->setSelectedChangedCallback([this, system, core_choice](std::string emulatorName)
    {
        std::string currentCore = Settings::getInstance()->getString(system->getName() + ".core");
        std::string defaultCore;
        
        for (auto& emulator : system->getSystemEnvData()->mEmulators)
        {
            if (emulatorName == emulator.mName)
            {
                for (auto core : emulator.mCores)
                {
                    defaultCore = core;
                    break;
                }
            }
        }

        core_choice->clear();

    //  if (defaultCore.length() == 0)
            core_choice->add(_("AUTO"), "", false);
    //  else
    //      core_choice->add(_("AUTO") + " (" + defaultCore + ")", "", false);

        std::vector<std::string> cores = system->getSystemEnvData()->getCores(emulatorName);

        bool found = false;

        for (auto it = cores.begin(); it != cores.end(); it++)
        {
            std::string core = *it;
            core_choice->add(core, core, currentCore == core);
            if (currentCore == core)
                found = true;
        }

        if (!found)
            core_choice->selectFirstItem();
        else
            core_choice->invalidate();
    });

    row.elements.clear();
    row.addElement(std::make_shared<TextComponent>(mWindow, "CORE", theme->Text.font, theme->Text.color), true);
    row.addElement(core_choice, false);
    s->addRow(row);

    // force change event to load core list
    emul_choice->invalidate();


    s->addSaveFunc([system, emul_choice, core_choice]
    {       
        Settings::getInstance()->setString(system->getName() + ".emulator", emul_choice->getSelected());
        Settings::getInstance()->setString(system->getName() + ".core", core_choice->getSelected());
    });

    mWindow->pushGui(s);
}

void GuiMenu::openSystemPlaylistsSettings(SystemData* system)
{
    auto theme = ThemeData::getMenuTheme();

    GuiSettings* s = new GuiSettings(mWindow, std::string("PLAYLIST PATH : ") + system->getFullName().c_str());

    s->addEntry(_("SELECT PLAYLIST FOLDER").c_str(), true, [this, system] { openSystemPlaylistsSelection(system); });

    auto writeSystemPlaylistPath = [this, system](const std::string& newVal) {
        // we need to store the first Gui and remove it, as it'll be deleted by the actual Gui
        Window* window = mWindow;
        GuiComponent* topGui = window->peekGui();
        window->removeGui(topGui);
        //set back Text color
        //theme->Text.color = Settings::getInstance()->getInt(std::string("PlaylistFolder_text_color")); 
    
        std::string playlistPath = newVal;
        Settings::getInstance()->setString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str(), playlistPath);
        Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), true);
        Settings::getInstance()->saveFile();
        //Stop music to avoid jumping songs when getting out sound menu
        //AudioManager::getInstance()->stopMusic();
	    //ViewController::get()->reloadAll();
    };
    s->addEntry(_("CUSTOM PLAYLIST PATH").c_str(), true, [this, writeSystemPlaylistPath, s] {

        //Si cambio el color de texto del  tema no se puede volver a poner inmediatamente
        //theme->Text.color              = 0x000000FF;
        if (Settings::getInstance()->getBool("UseOSK")) {
            mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("CUSTOM PATH"), "", writeSystemPlaylistPath , false));
        }
        else {
            mWindow->pushGui(new GuiTextEditPopup(mWindow, _("CUSTOM PATH"), "", writeSystemPlaylistPath, false));
        }
        s->setVariable("reloadAll", true);
    });

    mWindow->pushGui(s);
}

void GuiMenu::openFolderPlaylistsSettings(std::string folder)
{
    auto theme = ThemeData::getMenuTheme();

    GuiSettings* s = new GuiSettings(mWindow, std::string("PLAYLIST SYSTEMS PER FOLDER: ") + Utils::FileSystem::getFileName(folder));

    auto displayedSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("VISIBLE SYSTEMS"), true);

    for (auto system : SystemData::sSystemVector)
    {
        //Verify folder 
        std::string systemfolderConfig = Settings::getInstance()->getString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str());
        if (!Utils::FileSystem::isDirectory(systemfolderConfig) and !Utils::FileSystem::isDirectory(systemfolderConfig))
        {
            Settings::getInstance()->setString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str(), std::string("/home/arcade/.emulationstation/music") );
            Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), false);
        }

        if(!system->isCollection() && !system->isGroupChildSystem())
            displayedSystems->add(system->getThemeFolder().c_str() , system, Settings::getInstance()->getString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str())==folder );  
    }
    s->addWithLabel(_("VISIBLE SYSTEMS"), displayedSystems);
    s->addSaveFunc([this, folder, displayedSystems]
    {
        std::vector<SystemData*> sysObjs = displayedSystems->getSelectedObjects();

        for (auto system : SystemData::sSystemVector)
        {
            if (system->isCollection() || system->isGroupChildSystem())
                continue;

			for(auto it = sysObjs.cbegin(); it != sysObjs.cend(); it++)
			{
				if ((*it)->getThemeFolder().c_str() == system->getThemeFolder().c_str())
				{
                   //system assign folder path 
                   Settings::getInstance()->setString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str(), folder );
                   Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), false);
                }
            }
        }
        Settings::getInstance()->saveFile();
        //Stop music to avoid jumping songs when getting out sound menu
        //AudioManager::getInstance()->stopMusic();
        //ViewController::get()->reloadAll(mWindow);
        ViewController::get()->ReloadSoundSystem();
        //AudioManager::getInstance()->themeChanged(ViewController::get()->getState().getSystem()->getTheme(), true, false);
        //AudioManager::getInstance()->stopMusic();
    });


    mWindow->pushGui(s);
}

void GuiMenu::openSystemPlaylistsSelection(SystemData* system)
{
    auto theme = ThemeData::getMenuTheme();
    GuiSettings* s = new GuiSettings(mWindow, std::string("PLAYLIST PATH : ") + system->getFullName().c_str());

    auto folder_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SELECT FOLDER"), false);

    std::list<std::string> folderContentList = Utils::FileSystem::getDirContent(std::string("/home/arcade/.emulationstation/music_folders/"));
    std::list<std::string>::iterator it = folderContentList.begin();

    //Get current config
    std::string currentFolderConfig = Settings::getInstance()->getString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str());
    //Verify folder
    if (!Utils::FileSystem::isDirectory(currentFolderConfig) and !Utils::FileSystem::isDirectory(currentFolderConfig))
    {
        Settings::getInstance()->setString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str(), std::string("/home/arcade/.emulationstation/music") );
        Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), false);
        currentFolderConfig = std::string("/home/arcade/.emulationstation/music");
    }

    folder_choice->add(_("'USER DEFAULT'"), std::string("/home/arcade/.emulationstation/music"), false );

    if (Settings::getInstance()->getBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str()))
        folder_choice->add(_("'SHOW CUSTOM PATH'"), currentFolderConfig , false );

    while(it != folderContentList.end())
    {
        bool isDirectory  = Utils::FileSystem::isDirectory(*it);
        bool isSymlink    = Utils::FileSystem::isSymlink  (*it); 
        if (isDirectory or isSymlink)
            folder_choice->add(Utils::FileSystem::getFileName(*it), std::string("/home/arcade/.emulationstation/music_folders/") + Utils::FileSystem::getFileName(*it), currentFolderConfig == *it);
        it++;
    }
    bool has_selection = folder_choice->hasSelection();
    if (!has_selection and  Settings::getInstance()->getBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str()))
    {
        folder_choice->selectSecondItem(); //Re-select custom path
        Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), true);
    } else if (!has_selection) {
        folder_choice->selectFirstItem();  //Select default path
        Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), false);
    }    

    ComponentListRow row;
    row.addElement(std::make_shared<TextComponent>(mWindow, _("SELECT USER PLAYLIST FOLDER"), theme->Text.font, theme->Text.color), true);
    row.addElement(folder_choice, false);

    s->addRow(row);
    s->addSaveFunc([this, system, folder_choice, currentFolderConfig]
    {       
        if ( (Settings::getInstance()->getBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str())) and (folder_choice->getSelNum()==2)) 
        {
		    mWindow->displayNotificationMessage(std::string("CUSTOM PATH: ") +currentFolderConfig);
            Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), true);
        } else {
            if (std::string("/home/arcade/.emulationstation/music")==folder_choice->getSelected())
            {
                Settings::getInstance()->setString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str(), std::string("/home/arcade/.emulationstation/music") );
                Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), false);
            } else {
                Settings::getInstance()->setString(std::string("PlaylistFolder_") + system->getThemeFolder().c_str(), folder_choice->getSelected());
                Settings::getInstance()->setBool(std::string("CustomPlaylistFolder_") + system->getThemeFolder().c_str(), false);
            }
        }
        Settings::getInstance()->saveFile();
        //Stop music to avoid jumping songs when getting out sound menu
        ///AudioManager::getInstance()->stopMusic();
        ViewController::get()->ReloadSoundSystem();
        //AudioManager::getInstance()->themeChanged(ViewController::get()->getState().getSystem()->getTheme(), true, false);
        //AudioManager::getInstance()->stopMusic();
    });

    mWindow->pushGui(s);
}

void GuiMenu::openEmulatorSettings()
{
    GuiSettings* configuration = new GuiSettings(mWindow, _("EMULATOR SETTINGS").c_str());  

    Window* window = mWindow;
    
    // For each activated system
    for (auto system : SystemData::sSystemVector)
    {
        if (system->isCollection())
            continue;

        if (system->getSystemEnvData()->mEmulators.size() == 0)
            continue;

        if (system->getSystemEnvData()->mEmulators.size() == 1 && system->getSystemEnvData()->mEmulators[0].mCores.size() <= 1)
            continue;
        
        configuration->addEntry(system->getFullName(), true, [this, system] { openSystemEmulatorSettings(system); });
    }

    window->pushGui(configuration);
}


void GuiMenu::openSystemSpecificMusicFolderSettings()
{
    GuiSettings* configuration = new GuiSettings(mWindow, _("MUSIC FOLDER SETTINGS").c_str());  

    Window* window = mWindow;
    
    bool anyPlaylistFolder = false;
    std::list<std::string> availableFolderList = Utils::FileSystem::getDirContent(std::string("/home/arcade/.emulationstation/music_folders/"));
    std::list<std::string>::iterator it = availableFolderList.begin();
    while(it != availableFolderList.end())
    {
        if (Utils::FileSystem::isDirectory(*it) or Utils::FileSystem::isSymlink)
            anyPlaylistFolder = true;
            break;
        it++;
    }

    if (anyPlaylistFolder)
    {
        configuration->addEntry(_("PLAYLIST SETTINGS PER-SYSTEM "), true, [this] { openPerSystemsPlaylistSettings(); }, "iconSystem");
        configuration->addEntry(_("PLAYLIST SETTINGS PER-FOLDER "), true, [this] { openPerFolderPlaylistSettings(); }, "iconSystem");
    } else {
        std::string msg = "";
        if (Settings::getInstance()->getString("Language")=="es" ||
            Settings::getInstance()->getString("Language")=="Es" || 
            Settings::getInstance()->getString("Language")=="ES" || 
            Settings::getInstance()->getString("Language")=="español" || 
            Settings::getInstance()->getString("Language")=="Español" || 
            Settings::getInstance()->getString("Language")=="ESPAÑOL" || 
            Settings::getInstance()->getString("Language")=="castellano" || 
            Settings::getInstance()->getString("Language")=="Castellano" || 
            Settings::getInstance()->getString("Language")=="CASTELLANO" 
           ) {
            msg += " La carpeta de Playlists está vacia :\n";
            msg += " /home/arcade/.emulationstation/music_folders/\n";
            msg += " \n";
            msg += " 1) Puedes crear carpetas desde este menú.\n";
            msg += " 2) Mete archivos de música en las carpetas\n";
            msg += "    desde el SO o a través de la red.\n";
            msg += " 3) Asigna las carpetas a los sistemas.\n";
            msg += " \n";
            msg += " El borrado de carpetas es responsabilidad \n";
            msg += " del usuario.\n";
        } else if (Settings::getInstance()->getString("Language")=="ca" ||
                   Settings::getInstance()->getString("Language")=="CA" || 
                   Settings::getInstance()->getString("Language")=="catala" || 
                   Settings::getInstance()->getString("Language")=="català" || 
                   Settings::getInstance()->getString("Language")=="Catala" || 
                   Settings::getInstance()->getString("Language")=="Català" || 
                   Settings::getInstance()->getString("Language")=="CATALA" || 
                   Settings::getInstance()->getString("Language")=="CATALÀ"
                  ) {
            msg += " La carpeta de Playlists està buida :\n";
            msg += " /home/arcade/.emulationstation/music_folders/\n";
            msg += " \n";
            msg += " 1) Pots crear carpetas desde el menú.\n";
            msg += " 2) Fica arxius de música a las carpetes\n";
            msg += "    desde el SO o per xarxa.\n";
            msg += " 3) Assigna les carpetes als sistemes.\n";
            msg += " \n";
            msg += " L'usuari és responsable d'esborrar les carpetes!\n";

        } else {
            msg += " The Playlists Folder is empty:\n";
            msg += " /home/arcade/.emulationstation/music_folders/\n";
            msg += " \n";
            msg += " 1) You could create folders from here.\n";
            msg += " 2) Put music files into these folders from the OS\n";
            msg += "    or transfer files over your network.\n";
            msg += " 3) Assign folders to the systems.\n";
            msg += " \n";
            msg += " The user is responsible of removing folders!\n";
        }
        window->pushGui(new GuiMsgBox(window, _(msg), _("OK"))); 

    }

    auto createUserPlaylistFolder= [this](const std::string& newVal) {
        // we need to store the first Gui and remove it, as it'll be deleted by the actual Gui
        Window* window = mWindow;
        GuiComponent* topGui = window->peekGui();
        window->removeGui(topGui);
        //create music folder
        std::string playlistPath = newVal;
        Utils::FileSystem::createDirectory(std::string("/home/arcade/.emulationstation/music_folders/")+playlistPath);
    };
    configuration->addEntry(_("SHOW AVAILABLE PLAYLIST FOLDERS ").c_str(), true, [this] { openShowPlaylistFolders(); });
    configuration->addEntry(_("CREATE USER PLAYLIST FOLDER ").c_str(), true, [this, createUserPlaylistFolder] {
        if (Settings::getInstance()->getBool("UseOSK")) {
            mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("PLAYLIST FOLDER NAME"), "", createUserPlaylistFolder, false));
        }
        else {
            mWindow->pushGui(new GuiTextEditPopup(mWindow, _("PLAYLIST FOLDER NAME"), "", createUserPlaylistFolder, false));
        }
    });


    window->pushGui(configuration);
}

void GuiMenu::openPerSystemsPlaylistSettings()
{
    GuiSettings* configuration = new GuiSettings(mWindow, _("PLAYLIST SETTINGS PER-SYSTEM: SELECT MUSIC FOLDER").c_str());    

    Window* window = mWindow;
    
    // For each activated system
    for (auto system : SystemData::sSystemVector)
    {
        if (system->isCollection())
            continue;

        configuration->addEntry(system->getFullName(), true, [this, system] { openSystemPlaylistsSettings(system); });
    }

    window->pushGui(configuration);
}

void GuiMenu::openShowPlaylistFolders()
{
    GuiSettings* configuration = new GuiSettings(mWindow, _("AVAILABLE PLAYLIST FOLDERS").c_str());    

    Window* window = mWindow;
    

    std::list<std::string> folderContentList = Utils::FileSystem::getDirContent(std::string("/home/arcade/.emulationstation/music_folders/"));
    std::list<std::string>::iterator it = folderContentList.begin();

    while(it != folderContentList.end())
    {
        std::string folder = *it; 
        std::string thisFolderName = Utils::FileSystem::getFileName(*it);
        if (Utils::FileSystem::isDirectory(*it) or Utils::FileSystem::isSymlink)
            configuration->addEntry(Utils::FileSystem::getFileName(folder), true, [this, folder] {openShowPlaylistFolderContent(folder); });
        it++;
    }
    window->pushGui(configuration);
}

void GuiMenu::openShowPlaylistFolderContent(std::string folder)
{
    GuiSettings* configuration = new GuiSettings(mWindow, _("RECURSIVE PLAYLIST FOLDER CONTENT").c_str());    

    Window* window = mWindow;
    
    bool recursive = true;
    std::list<std::string> folderContentList = Utils::FileSystem::getDirContent(folder, recursive);
    std::list<std::string>::iterator it = folderContentList.begin();

    while(it != folderContentList.end())
    {
        std::string thisFile = *it;
        std::string thisFileName = Utils::FileSystem::getFileName(*it);
        if (Utils::FileSystem::isRegularFile(*it))
            configuration->addEntry(thisFileName, false, [this, thisFile] {
                AudioManager::getInstance()->stopMusic();
                AudioManager::getInstance()->playMusic(thisFile); 
            });
        it++;
    }
    window->pushGui(configuration);
}

void GuiMenu::openPerFolderPlaylistSettings()
{
    GuiSettings* configuration = new GuiSettings(mWindow, _("PLAYLIST SETTINGS PER-FOLDER: SELECT SYSTEMS ").c_str());    

    Window* window = mWindow;
    
    std::list<std::string> folderContentList = Utils::FileSystem::getDirContent(std::string("/home/arcade/.emulationstation/music_folders/"));
    std::list<std::string>::iterator it = folderContentList.begin();

    while(it != folderContentList.end())
    {
        std::string folder = *it; 
        std::string thisFolderName = Utils::FileSystem::getFileName(*it);
        if (Utils::FileSystem::isDirectory(*it) or Utils::FileSystem::isSymlink)
            configuration->addEntry(Utils::FileSystem::getFileName(folder), true, [this, folder] { openFolderPlaylistsSettings(folder); });
        it++;
    }

    window->pushGui(configuration);
}


void GuiMenu::openUpdateSettings()
{
    Window* window = mWindow;
    auto s = new GuiSettings(mWindow, _("DOWNLOADS AND UPDATES"));

    // themes installer/browser
    s->addEntry(_("THEME INSTALLER"), true, [this]
    {
        mWindow->pushGui(new GuiThemeInstall(mWindow));
    });

    // Enable updates
    auto updates_enabled = std::make_shared<SwitchComponent>(mWindow);
    updates_enabled->setState(Settings::getInstance()->getBool("updates.enabled"));
    s->addWithLabel(_("AUTO UPDATES"), updates_enabled);
    s->addSaveFunc([updates_enabled]
    {
        Settings::getInstance()->setBool("updates.enabled", updates_enabled->getState());
    });

    // Start update
    s->addEntry(ApiSystem::state == UpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this, s]
    {
        if (ApiSystem::checkUpdateVersion().empty())
        {
            mWindow->pushGui(new GuiMsgBox(mWindow, _("NO UPDATE AVAILABLE")));
            return;
        }

        if (ApiSystem::state == UpdateState::State::UPDATE_READY)
        {
            if (quitES(QuitMode::QUIT))
                LOG(LogWarning) << "Reboot terminated with non-zero result!";
        }
        else if (ApiSystem::state == UpdateState::State::UPDATER_RUNNING)
            mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATE IS ALREADY RUNNING")));
        else
        {
            ApiSystem::startUpdate(mWindow);

            s->setVariable("closeGuiMenu", true);
            s->close();         
        }
    });

    s->updatePosition();

    auto pthis = this;

    s->onFinalize([s, pthis, window]
    {
        if (s->getVariable("closeGuiMenu"))
            delete pthis;
    });

    mWindow->pushGui(s);

}
    

void GuiMenu::openOtherSettings()
{
    Window* window = mWindow;
    auto s = new GuiSettings(mWindow, _("ADVANCED SETTINGS"));

    /*
    // Emulator settings 
    for (auto system : SystemData::sSystemVector)
    {
        if (system->isCollection() || system->getSystemEnvData()->mEmulators.size() == 0 || (system->getSystemEnvData()->mEmulators.size() == 1 && system->getSystemEnvData()->mEmulators[0].mCores.size() <= 1))
            continue;

        s->addEntry(_("EMULATOR SETTINGS"), true, [this] { openEmulatorSettings(); }, "iconGames");
        break;
    }
    */
    // power saver
    auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER SAVER MODES"), false);
    std::vector<std::string> modes;
    modes.push_back("disabled");
    modes.push_back("default");
    modes.push_back("enhanced");
    modes.push_back("instant");
    for (auto it = modes.cbegin(); it != modes.cend(); it++)
        power_saver->add(_(it->c_str()), *it, Settings::getInstance()->getString("PowerSaverMode") == *it);

    s->addWithLabel(_("POWER SAVER MODES"), power_saver);
    s->addSaveFunc([this, power_saver] {
        if (Settings::getInstance()->getString("PowerSaverMode") != "instant" && power_saver->getSelected() == "instant") {
            Settings::getInstance()->setString("TransitionStyle", "instant");
            Settings::getInstance()->setString("GameTransitionStyle", "instant");
            Settings::getInstance()->setBool("MoveCarousel", false);
            Settings::getInstance()->setBool("EnableSounds", false);
        }

        GuiComponent::ALLOWANIMATIONS = Settings::getInstance()->getString("TransitionStyle") != "instant";

        Settings::getInstance()->setString("PowerSaverMode", power_saver->getSelected());
        PowerSaver::init();
    });


    // LANGUAGE

    std::vector<std::string> langues;
    langues.push_back("en");

    std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/splash.svg");
    if (xmlpath.length() > 0)
    {
        xmlpath = Utils::FileSystem::getParent(xmlpath) + "/locale/";

        Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(xmlpath, true);
        for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
        {
            if (Utils::FileSystem::isDirectory(*it))
                continue;

            std::string name = *it;

            if (name.rfind("emulationstation2.po") == std::string::npos)
                continue;

            name = Utils::FileSystem::createRelativePath(name, xmlpath, false);
            if (Utils::String::startsWith(name, "./"))
            {
                name = name.substr(2);

                while (name.find("/") != std::string::npos)
                    name = Utils::FileSystem::getParent(name);
            }
            else
                name = Utils::FileSystem::getParent(name);

            name = Utils::FileSystem::getFileName(name);

            if (name != "en")
                langues.push_back(name);
        }

        if (langues.size() > 1)
        {
            auto language = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LANGUAGE"), false);

            for (auto it = langues.cbegin(); it != langues.cend(); it++)
                language->add(*it, *it, Settings::getInstance()->getString("Language") == *it);

            s->addWithLabel(_("LANGUAGE"), language);
            s->addSaveFunc([language, window, s] {

                if (language->getSelected() != Settings::getInstance()->getString("Language"))
                {
                    if (Settings::getInstance()->setString("Language", language->getSelected()))
                        s->setVariable("reloadGuiMenu", true);
                }
            });
        }
    }


    // maximum vram
    auto max_vram = std::make_shared<SliderComponent>(mWindow, 40.f, 1000.f, 10.f, "Mb");
    max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
    s->addWithLabel(_("VRAM LIMIT"), max_vram);
    s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)Math::round(max_vram->getValue())); });


    /*
#if WIN32

    // Enable updates
    auto updates_enabled = std::make_shared<SwitchComponent>(mWindow);
    updates_enabled->setState(Settings::getInstance()->getBool("updates.enabled"));
    s->addWithLabel(_("AUTO UPDATES"), updates_enabled);
    s->addSaveFunc([updates_enabled]
    {
        Settings::getInstance()->setBool("updates.enabled", updates_enabled->getState());
    });

    // Start update
    s->addEntry(ApiSystem::state == UpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this]
    {
        if (ApiSystem::checkUpdateVersion().empty())
        {
            mWindow->pushGui(new GuiMsgBox(mWindow, _("NO UPDATE AVAILABLE")));
            return;
        }

        if (ApiSystem::state == UpdateState::State::UPDATE_READY)
        {
            if (quitES(QuitMode::QUIT))
                LOG(LogWarning) << "Reboot terminated with non-zero result!";
        }
        else if (ApiSystem::state == UpdateState::State::UPDATER_RUNNING)
            mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATE IS ALREADY RUNNING")));
        else
            ApiSystem::startUpdate(mWindow);
    });
#endif
*/




    // gamelists
    auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
    save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
    s->addWithLabel(_("SAVE METADATA ON EXIT"), save_gamelists);
    s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

    auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
    parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
    s->addWithLabel(_("PARSE GAMESLISTS ONLY"), parse_gamelists);
    s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });
    
#ifndef WIN32
    auto local_art = std::make_shared<SwitchComponent>(mWindow);
    local_art->setState(Settings::getInstance()->getBool("LocalArt"));
    s->addWithLabel(_("SEARCH FOR LOCAL ART"), local_art);
    s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });
#endif

#ifdef _RPI_
    // Video Player - VideoOmxPlayer
    auto omx_player = std::make_shared<SwitchComponent>(mWindow);
    omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
    s->addWithLabel("USE OMX PLAYER (HW ACCELERATED)", omx_player);
    s->addSaveFunc([omx_player]
    {
        // need to reload all views to re-create the right video components
        bool needReload = false;
        if(Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
            needReload = true;

        Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

        if(needReload)
            ViewController::get()->reloadAll();
    });

#endif

    // preload UI
    auto preloadUI = std::make_shared<SwitchComponent>(mWindow);
    preloadUI->setState(Settings::getInstance()->getBool("PreloadUI"));
    s->addWithLabel(_("PRELOAD UI"), preloadUI);
    s->addSaveFunc([preloadUI] { Settings::getInstance()->setBool("PreloadUI", preloadUI->getState()); });
    
    // optimizeVram
    auto optimizeVram = std::make_shared<SwitchComponent>(mWindow);
    optimizeVram->setState(Settings::getInstance()->getBool("OptimizeVRAM"));
    s->addWithLabel(_("OPTIMIZE IMAGES VRAM USE"), optimizeVram);
    s->addSaveFunc([optimizeVram]
    {
        TextureData::OPTIMIZEVRAM = optimizeVram->getState();
        Settings::getInstance()->setBool("OptimizeVRAM", optimizeVram->getState());
    });

#ifdef WIN32
    // vsync
    auto vsync = std::make_shared<SwitchComponent>(mWindow);
    vsync->setState(Settings::getInstance()->getBool("VSync"));
    s->addWithLabel(_("VSYNC"), vsync);
    s->addSaveFunc([vsync] 
    { 
        Settings::getInstance()->setBool("VSync", vsync->getState()); 
        Renderer::setSwapInterval();
    });
#endif

    // framerate    
    auto framerate = std::make_shared<SwitchComponent>(mWindow);
    framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
    s->addWithLabel(_("SHOW FRAMERATE"), framerate);
    s->addSaveFunc([framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });

    // threaded loading
    auto threadedLoading = std::make_shared<SwitchComponent>(mWindow);
    threadedLoading->setState(Settings::getInstance()->getBool("ThreadedLoading"));
    s->addWithLabel(_("THREADED LOADING"), threadedLoading);
    s->addSaveFunc([threadedLoading] { Settings::getInstance()->setBool("ThreadedLoading", threadedLoading->getState()); });

#ifndef _RPI_
    // full exit
    auto fullExitMenu = std::make_shared<SwitchComponent>(mWindow);
    fullExitMenu->setState(!Settings::getInstance()->getBool("ShowOnlyExit"));
    s->addWithLabel(_("COMPLETE QUIT MENU"), fullExitMenu);
    s->addSaveFunc([fullExitMenu] { Settings::getInstance()->setBool("ShowOnlyExit", !fullExitMenu->getState()); });
#endif

    // log level
    auto logLevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOG LEVEL"), false);
    std::vector<std::string> levels;
    levels.push_back("default");
    levels.push_back("disabled");
    levels.push_back("warning");
    levels.push_back("error");
    levels.push_back("debug");

    auto level = Settings::getInstance()->getString("LogLevel");
    if (level.empty())
        level = "default";

    for (auto it = levels.cbegin(); it != levels.cend(); it++)
        logLevel->add(_(it->c_str()), *it, level == *it);

    s->addWithLabel(_("LOG LEVEL"), logLevel);
    s->addSaveFunc([this, logLevel]
    {
        if (Settings::getInstance()->setString("LogLevel", logLevel->getSelected() == "default" ? "" : logLevel->getSelected()))
        {
            Log::setupReportingLevel();
            Log::init();
        }
    });




    s->updatePosition();

    auto pthis = this;

    s->onFinalize([s, pthis, window]
    {
        if (s->getVariable("reloadGuiMenu"))
        {
            delete pthis;
            window->pushGui(new GuiMenu(window, false));
        }
    });

    mWindow->pushGui(s);

}

void GuiMenu::openConfigInput()
{
    Window* window = mWindow;
//  window->pushGui(new GuiDetectDevice(window, false, nullptr));
        
    window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO CONFIGURE INPUT?"), _("YES"),
        [window] {
        window->pushGui(new GuiDetectDevice(window, false, nullptr));
    }, _("NO"), nullptr)
    );

}

void GuiMenu::openQuitMenu()
{
    if (Settings::getInstance()->getBool("ShowOnlyExit"))
    {
        Scripting::fireEvent("quit");
        quitES();
        return;
    }

    auto s = new GuiSettings(mWindow, _("QUIT"));

    Window* window = mWindow;

    ComponentListRow row;
    if (UIModeController::getInstance()->isUIModeFull())
    {
#ifndef WIN32
        // Restart does not work on Windows
        row.makeAcceptInputHandler([window] {
            window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
                [] {
                Scripting::fireEvent("quit");
                if(quitES(QuitMode::RESTART) != 0)
                    LOG(LogWarning) << "Restart terminated with non-zero result!";
            }, _("NO"), nullptr));
        });
        row.addElement(std::make_shared<TextComponent>(window, _("RESTART EMULATIONSTATION"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
        s->addRow(row);
#endif

        if(Settings::getInstance()->getBool("ShowExit"))
        {
            row.elements.clear();
            row.makeAcceptInputHandler([window] {
                window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), _("YES"),
                    [] {
                    Scripting::fireEvent("quit");
                    quitES();
                }, _("NO"), nullptr));
            });
            row.addElement(std::make_shared<TextComponent>(window, _("QUIT EMULATIONSTATION"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
            s->addRow(row);
        }
    }
    row.elements.clear();
    row.makeAcceptInputHandler([window] {
        window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
            [] {
            Scripting::fireEvent("quit", "reboot");
            Scripting::fireEvent("reboot");
            if (quitES(QuitMode::REBOOT) != 0)
                LOG(LogWarning) << "Restart terminated with non-zero result!";
        }, _("NO"), nullptr));
    });
    row.addElement(std::make_shared<TextComponent>(window, _("RESTART SYSTEM"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
    s->addRow(row);

    row.elements.clear();
    row.makeAcceptInputHandler([window] {
        window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), _("YES"),
            [] {
            Scripting::fireEvent("quit", "shutdown");
            Scripting::fireEvent("shutdown");
            if (quitES(QuitMode::SHUTDOWN) != 0)
                LOG(LogWarning) << "Shutdown terminated with non-zero result!";
        }, _("NO"), nullptr));
    });
    row.addElement(std::make_shared<TextComponent>(window, _("SHUTDOWN SYSTEM"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
    s->addRow(row);

    s->updatePosition();
    mWindow->pushGui(s);
}

std::string getBuildTime()
{
    std::string datestr = __DATE__;
    std::string timestr = __TIME__;

    std::istringstream iss_date(datestr);
    std::string str_month;
    int day;
    int year;
    iss_date >> str_month >> day >> year;

    int month;
    if (str_month == "Jan") month = 1;
    else if (str_month == "Feb") month = 2;
    else if (str_month == "Mar") month = 3;
    else if (str_month == "Apr") month = 4;
    else if (str_month == "May") month = 5;
    else if (str_month == "Jun") month = 6;
    else if (str_month == "Jul") month = 7;
    else if (str_month == "Aug") month = 8;
    else if (str_month == "Sep") month = 9;
    else if (str_month == "Oct") month = 10;
    else if (str_month == "Nov") month = 11;
    else if (str_month == "Dec") month = 12;
    else exit(-1);

    for (std::string::size_type pos = timestr.find(':'); pos != std::string::npos; pos = timestr.find(':', pos))
        timestr[pos] = ' ';

    std::istringstream iss_time(timestr);
    int hour, min, sec;
    iss_time >> hour >> min >> sec;

    char buffer[100];
    
    sprintf(buffer, "%4d%.2d%.2d%.2d%.2d%.2d\n", year, month, day, hour, min, sec);

    return buffer;
}

void GuiMenu::addVersionInfo()
{
    std::string  buildDate = getBuildTime();
    //  (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

    auto theme = ThemeData::getMenuTheme();
//  mVersion.setFont(Font::get(FONT_SIZE_SMALL));
//  mVersion.setColor(0x5E5E5EFF);

    mVersion.setFont(theme->Footer.font);
    mVersion.setColor(theme->Footer.color);

    mVersion.setLineSpacing(0);

#if WIN32
    std::string localVersion;
    std::string localVersionFile = Utils::FileSystem::getExePath() + "/version.info";
    if (Utils::FileSystem::exists(localVersionFile))
    {
        localVersion = Utils::FileSystem::readAllText(localVersionFile);
        localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");    
        mVersion.setText("EMULATIONSTATION V" + localVersion+" FCAMOD");    
    }
    else
#endif
        mVersion.setText("EMULATIONSTATION V" + Utils::String::toUpper(PROGRAM_VERSION_STRING) + " BUILD " + buildDate);

    mVersion.setHorizontalAlignment(ALIGN_CENTER);  
    mVersion.setVerticalAlignment(ALIGN_CENTER);
    addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() {
    mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow, _("SCREENSAVER SETTINGS")));
}

void GuiMenu::openCollectionSystemSettings() 
{
    if (ThreadedScraper::isRunning())
    {
        mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHEN SCRAPING IS RUNNING")));
        return;
    }

    mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::onSizeChanged()
{
    float h = mMenu.getButtonGridHeight();

    mVersion.setSize(mSize.x(), h);
    mVersion.setPosition(0, mSize.y() - h); //  mVersion.getSize().y()
}

void GuiMenu::addEntry(std::string name, bool add_arrow, const std::function<void()>& func, const std::string iconName)
{
    auto theme = ThemeData::getMenuTheme();
    std::shared_ptr<Font> font = theme->Text.font;
    unsigned int color = theme->Text.color;

    // populate the list
    ComponentListRow row;

    if (!iconName.empty())
    {
        std::string iconPath = theme->getMenuIcon(iconName);
        if (!iconPath.empty())
        {
            // icon
            auto icon = std::make_shared<ImageComponent>(mWindow);
            icon->setImage(iconPath);
            icon->setColorShift(theme->Text.color);
            icon->setResize(0, theme->Text.font->getLetterHeight() * 1.25f);
            row.addElement(icon, false);

            // spacer between icon and text
            auto spacer = std::make_shared<GuiComponent>(mWindow);
            spacer->setSize(10, 0);
            row.addElement(spacer, false);
        }
    }

    row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

    if (add_arrow)
    {
        std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
        row.addElement(bracket, false);
    }

    row.makeAcceptInputHandler(func);
    mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
    if(GuiComponent::input(config, input))
        return true;

    if((config->isMappedTo("b", input) || config->isMappedTo("start", input)) && input.value != 0)
    {
        delete this;
        return true;
    }

    return false;
}

HelpStyle GuiMenu::getHelpStyle()
{
    HelpStyle style = HelpStyle();

    if (ThemeData::getDefaultTheme() != nullptr)
    {
        std::shared_ptr<ThemeData> theme = std::shared_ptr<ThemeData>(ThemeData::getDefaultTheme(), [](ThemeData*) {});
        style.applyTheme(theme, "system");
    }
    else 
        style.applyTheme(ViewController::get()->getState().getSystem()->getTheme(), "system");

    return style;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
    std::vector<HelpPrompt> prompts;
    prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
    prompts.push_back(HelpPrompt("a", _("SELECT")));
    prompts.push_back(HelpPrompt("start", _("CLOSE")));
    return prompts;
}

void GuiMenu::createInputTextRow(GuiSettings *gui, std::string title, const char *settingsID, bool password)
{
    auto theme = ThemeData::getMenuTheme();
    std::shared_ptr<Font> font = theme->Text.font;
    unsigned int color = theme->Text.color;

    // LABEL
    Window *window = mWindow;
    ComponentListRow row;

    auto lbl = std::make_shared<TextComponent>(window, title, font, color);
    row.addElement(lbl, true); // label

    std::shared_ptr<GuiComponent> ed;

    std::string value = Settings::getInstance()->getString(settingsID);

    ed = std::make_shared<TextComponent>(window, ((password && value != "") ? "*********" : value), font, color, ALIGN_RIGHT); // Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT)
    row.addElement(ed, true);

    auto spacer = std::make_shared<GuiComponent>(mWindow);
    spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
    row.addElement(spacer, false);

    auto bracket = std::make_shared<ImageComponent>(mWindow);
    bracket->setImage(theme->Icons.arrow);
    bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
    row.addElement(bracket, false);

    auto updateVal = [ed, settingsID, password](const std::string &newVal) {
        if (!password)
            ed->setValue(newVal);
        else {
            ed->setValue("*********");
        }

        Settings::getInstance()->setString(settingsID, newVal);
    }; // ok callback (apply new value to ed)

    row.makeAcceptInputHandler([this, title, updateVal, settingsID]
    {
        std::string data = Settings::getInstance()->getString(settingsID);
        mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, title, data, updateVal, false));
    });

    gui->addRow(row);
}
