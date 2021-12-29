#include "AudioManager.h"

#include "Log.h"
#include "Settings.h"
#include "Sound.h"
#include <SDL.h>
#include <time.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <random>

#ifdef WIN32
#include <time.h>
#else
#include <unistd.h>
#endif

std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;
std::shared_ptr<AudioManager> AudioManager::sInstance;

AudioManager::AudioManager() : mCurrentMusic(NULL), mInitialized(false), mMusicVolume(MIX_MAX_VOLUME), mVideoPlaying(false), mForceRandomization(true)
{	
	init();
}

AudioManager::~AudioManager()
{
	deinit();
}

std::shared_ptr<AudioManager> & AudioManager::getInstance()
{
	if (sInstance == nullptr)
		sInstance = std::shared_ptr<AudioManager>(new AudioManager);
	
	return sInstance;
}

bool AudioManager::isInitialized()
{
	if (sInstance == nullptr)
		return false;

	return sInstance->mInitialized;
}

void AudioManager::init()
{
	if (mInitialized)
		return;

	mRunningFromPlaylist = false;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
		return;
	}

	//Open the audio device and pause
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0)
		LOG(LogError) << "MUSIC Error - Unable to open SDLMixer audio: " << SDL_GetError() << std::endl;
	else
	{
		mInitialized = true;
		LOG(LogInfo) << "SDL AUDIO Initialized";

		// Reload sounds
		for (unsigned int i = 0; i < sSoundVector.size(); i++)
			sSoundVector[i]->init();
	}
    mCurrentRealThemeMusicDirectory  = "EMPTY"; 
    mPreviousRealThemeMusicDirectory = "EMPTY"; 
}

void AudioManager::deinit()
{
	if (!mInitialized)
		return;

	mInitialized = false;

	//stop all playback
	stop();
	stopMusic();

	// Stop playing all Sounds & reload them 
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
		sSoundVector[i]->deinit();

	Mix_HookMusicFinished(nullptr);
	Mix_HaltMusic();

	//completely tear down SDL audio. else SDL hogs audio resources and emulators might fail to start...
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioManager::registerSound(std::shared_ptr<Sound> & sound)
{
	getInstance();
	sSoundVector.push_back(sound);
}

void AudioManager::unregisterSound(std::shared_ptr<Sound> & sound)
{
	getInstance();
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
	{
		if (sSoundVector.at(i) == sound)
		{
			sSoundVector[i]->stop();
			sSoundVector.erase(sSoundVector.cbegin() + i);
			return;
		}
	}
	LOG(LogError) << "AudioManager Error - tried to unregister a sound that wasn't registered!";
}

void AudioManager::play()
{
	getInstance();
}

void AudioManager::stop()
{
	// Stop playing all Sounds
	for (unsigned int i = 0; i < sSoundVector.size(); i++)
		if (sSoundVector.at(i)->isPlaying())
			sSoundVector[i]->stop();
}

void AudioManager::findMusic(const std::string &path, std::vector<std::string>& all_matching_files) 
{
	if (!Utils::FileSystem::isDirectory(path))
		return;
	
    //Juanjo : He anulado esta función. Ya esta implicita en la estructura de carpetas que voy a 
    //         crear y en los links simbolicos. La forma anterior no permite compartir audios y 
    //         dejar una lista default de forma sencilla.
	//bool anySystem = !Settings::getInstance()->getBool("audio.persystem");
	bool findSubfolders= true;

	auto dirContent = Utils::FileSystem::getDirContent(path);
	for (auto it = dirContent.cbegin(); it != dirContent.cend(); ++it) 
	{
		if (Utils::FileSystem::isDirectory(*it)) 
		{
			if (*it == "." || *it == "..")
				continue;

			//if (anySystem || mSystemName == Utils::FileSystem::getFileName(*it))
			if (findSubfolders)
				findMusic(*it, all_matching_files);
		}
		else 
		{
			std::string extension = Utils::String::toLower(Utils::FileSystem::getExtension(*it));
			if (extension == ".mp3" || extension == ".ogg")
				all_matching_files.push_back(*it);			
		}		
	}
}

void AudioManager::playRandomMusic(bool continueIfPlaying, bool force) 
{
	if (!mInitialized)
		return;

	std::vector<std::string> musics;

	// check in Theme music directory
	if (!mCurrentThemeMusicDirectory.empty())
		findMusic(mCurrentThemeMusicDirectory, musics);

	// check in User music directory
	if (musics.empty() && !Settings::getInstance()->getString("UserMusicDirectory").empty())
		findMusic(Settings::getInstance()->getString("MusicDirectory"), musics);

	// check in System music directory
	if (musics.empty() && !Settings::getInstance()->getString("MusicDirectory").empty())
		findMusic(Settings::getInstance()->getString("MusicDirectory"), musics);

	// check in .emulationstation/music directory
	if (musics.empty()) {
		//findMusic(Utils::FileSystem::getHomePath() + "/.emulationstation/music", musics);
	    mCurrentThemeMusicDirectory = Utils::FileSystem::getHomePath() + "/.emulationstation/music";
		findMusic(mCurrentThemeMusicDirectory, musics);
	}
	if (musics.empty()) 
		return;

    int char_sz = mCurrentThemeMusicDirectory.length();
    char path_char_array[char_sz+1];
    strcpy(path_char_array,mCurrentThemeMusicDirectory.c_str());
    // Get rid of symbolic links to identify real path from /
	char * thisRealPath =  realpath(path_char_array,0); 	

    mCurrentRealThemeMusicDirectory  = std::string( thisRealPath );	 
	mMusicPathInfo= mCurrentRealThemeMusicDirectory ;	
	free(thisRealPath);
    if (!force && mCurrentRealThemeMusicDirectory == mPreviousRealThemeMusicDirectory)  
	   return;

    if (mCurrentRealThemeMusicDirectory != mPreviousRealThemeMusicDirectory) { 
        mPlayListItemIncrease = true;
        mPlayListItemDecrease = true;
        mPlayListItemPos = 0;
        mPreviousRealThemeMusicDirectory = mCurrentRealThemeMusicDirectory;
        mForceRandomization = true;
    }

    // continue playing ?
    if (mCurrentMusic != NULL && continueIfPlaying) 
    	return;

    std::string itemToBePlayed;                  
	if (Settings::getInstance()->getBool("RandomMusicList")) {

        //Randomize Index list if system has changed or forced
        if (mForceRandomization) 
        {
            mForceRandomization = false;
#if defined(WIN32)
    	    srand(time(NULL) % getpid());
#else
    	    srand(time(NULL) % getpid() + getppid());
#endif
            //Generate Index list
            randomPosList.resize(musics.size());
            for (int i=0; i<musics.size(); ++i) 
                randomPosList.at(i) = i;

            //Randomize Index list
            int n = randomPosList.size();
            for (int i = 0; i < n - 1; i++)
            {
                int j = i + rand() % (n - i);
                std::swap(randomPosList[i], randomPosList[j]);
            }
        }

        if (mPlayListItemIncrease){
            if (mPlayListItemPos == musics.size()-1)
                mPlayListItemPos = 0;
            else
                mPlayListItemPos++;

            mPlayListItemIncrease = false;
        }
        if (mPlayListItemDecrease){
            if (mPlayListItemPos == 0)
                mPlayListItemPos = musics.size()-1;
            else
                mPlayListItemPos--;

            mPlayListItemDecrease = false;
        }
        //mTestString = std::to_string(mPlayListItemPos);
        //itemToBePlayed = musics[randomPosList.at(mPlayListItemPos)];
        itemToBePlayed = musics[randomPosList[mPlayListItemPos]];

    } else {    
        mForceRandomization = true;
        if (mPlayListItemIncrease){
            if (mPlayListItemPos == musics.size()-1)
                mPlayListItemPos = 0;
            else
                mPlayListItemPos++;

            mPlayListItemIncrease = false;
        }
        if (mPlayListItemDecrease){
            if (mPlayListItemPos == 0)
                mPlayListItemPos = musics.size()-1;
            else
                mPlayListItemPos--;

            mPlayListItemDecrease = false;
        }
        //mTestString = std::to_string(mPlayListItemPos);
        itemToBePlayed = musics[mPlayListItemPos];
    }

	//playMusic(musics.at(randomIndex));
	playMusic(itemToBePlayed);
	mRunningFromPlaylist = true;
}

void AudioManager::increaseListPosition() 
{
  mPlayListItemIncrease = true;
} 

void AudioManager::decreaseListPosition() 
{
  mPlayListItemDecrease = true;
}

void AudioManager::playMusic(std::string path)
{
	if (!mInitialized)
		return;

	// free the previous music
	stopMusic();
		
	// load a new music
	mCurrentMusic = Mix_LoadMUS(path.c_str());
	if (mCurrentMusic == NULL)
	{
		LOG(LogError) << Mix_GetError() << " for " << path;
		return;
	}

	if (Mix_FadeInMusic(mCurrentMusic, 1, 1000) == -1) 
	{
		stopMusic();
		return;
	}

	Mix_HookMusicFinished(AudioManager::onMusicFinished);
	mCurrentMusicPath = path;

	mCurrentSong = Utils::FileSystem::getStem(path);
}

void AudioManager::onMusicFinished() 
{
    //Force playing music when music has stopped
	AudioManager::getInstance()->increaseListPosition();
	AudioManager::getInstance()->playRandomMusic(false, true);
}

void AudioManager::stopMusic() 
{
	if (mCurrentMusic == NULL)
		return;
	
	Mix_HookMusicFinished(nullptr);
	Mix_HaltMusic();
	Mix_FreeMusic(mCurrentMusic);
	
	mCurrentMusicPath = "";
	mCurrentMusic = NULL;	
}

void AudioManager::themeChanged(const std::shared_ptr<ThemeData>& theme, bool force, bool savedTheme)
{
	if (!savedTheme)
        lastTheme = theme;	

	if (!force && mSystemName == theme->getSystemThemeFolder())
		return;

	mSystemName = theme->getSystemThemeFolder();
	//mCurrentThemeMusicDirectory = "";

	if (!Settings::getInstance()->getBool("audio.bgmusic"))
		return;
    else
	    mCurrentThemeMusicDirectory = "";

	const ThemeData::ThemeElement* elem ;
	if (savedTheme)
	    elem = lastTheme->getElement("system", "directory", "sound");
    else
	    elem = theme->getElement("system", "directory", "sound");

	if (Settings::getInstance()->getBool("audio.persystem"))
	{
		//if (elem && elem->has("path")){
		//	mCurrentThemeMusicDirectory = elem->get<std::string>("path");
        //}
		mCurrentThemeMusicDirectory = Settings::getInstance()->getString(std::string("PlaylistFolder_") + mSystemName );

		std::string bgSound;

	    if (savedTheme)
		    elem = lastTheme->getElement("system", "bgsound", "sound");
        else
		    elem = theme->getElement("system", "bgsound", "sound");
		if (elem && elem->has("path") && Utils::FileSystem::exists(elem->get<std::string>("path")))
		{
			bgSound = Utils::FileSystem::getCanonicalPath(elem->get<std::string>("path"));
			if (bgSound == mCurrentMusicPath)
				return;
		}

		// Found a music for the system
		if (!bgSound.empty())
		{
            //Just in case
            mPreviousRealThemeMusicDirectory = bgSound;
			mRunningFromPlaylist = false;

            //Save music path for bgsound files
            int char_sz = bgSound.length();
            char path_char_array[char_sz+1];
            strcpy(path_char_array,bgSound.c_str());
            // Get rid of symbolic links to identify real path from /
	        char * thisRealPath =  realpath(path_char_array,0); 	
	        mMusicPathInfo= std::string( thisRealPath );	  

			playMusic(bgSound);
			return;
		}
	}


	mSystemName = theme->getSystemThemeFolder();
	playRandomMusic(false, force);	
    
}

void AudioManager::setVideoPlaying(bool state)
{
	if (sInstance == nullptr || !sInstance->mInitialized || !Settings::getInstance()->getBool("audio.bgmusic"))
		return;
	
	if (state && !Settings::getInstance()->getBool("VideoLowersMusic"))
		return;

	sInstance->mVideoPlaying = state;
}

void AudioManager::update(int deltaTime)
{
	if (sInstance == nullptr || !sInstance->mInitialized || !Settings::getInstance()->getBool("audio.bgmusic"))
		return;

	float deltaVol = deltaTime / 8.0f;

	#define MINVOL 5

	if (sInstance->mVideoPlaying && sInstance->mMusicVolume > MINVOL)
	{		
		sInstance->mMusicVolume -= deltaVol;
		if (sInstance->mMusicVolume < MINVOL)
			sInstance->mMusicVolume = MINVOL;

		Mix_VolumeMusic((int) sInstance->mMusicVolume);
	}
	else if (!sInstance->mVideoPlaying && sInstance->mMusicVolume < MIX_MAX_VOLUME)
	{
		sInstance->mMusicVolume += deltaVol;
		if (sInstance->mMusicVolume > MIX_MAX_VOLUME)
			sInstance->mMusicVolume = MIX_MAX_VOLUME;

		Mix_VolumeMusic((int)sInstance->mMusicVolume);
	}
}
