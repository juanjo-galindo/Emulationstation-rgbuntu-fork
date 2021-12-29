#pragma once
#ifndef ES_CORE_AUDIO_MANAGER_H
#define ES_CORE_AUDIO_MANAGER_H

#include <SDL_audio.h>
#include <memory>
#include <vector>
#include "SDL_mixer.h"
#include "ThemeData.h"
#include <string>
//Juanjo
#include<stdlib.h>
//#include<stdio.h>
class Sound;

class AudioManager
{	

public:
	static std::shared_ptr<AudioManager> & getInstance();
	static bool isInitialized();
	
	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound> & sound);
	void unregisterSound(std::shared_ptr<Sound> & sound);

	void play();
	void stop();

	void playRandomMusic(bool continueIfPlaying = true, bool force = false);
	void stopMusic();
	void playMusic(std::string path);
	void themeChanged(const std::shared_ptr<ThemeData>& theme, bool force=false, bool savedTheme=false);
    std::shared_ptr<ThemeData> lastTheme;

	void setSystemName(std::string name) {
		mSystemName = name;
	}

	std::string popSongName() 
	{ 		
		if (!mCurrentSong.empty())
		{
			std::string ret = mCurrentSong;
			mCurrentSong = "";
			return ret;
		}

		return "";
	}

	std::string popMusicPath() 
	{ 		
		if (!mMusicPathInfo.empty())
		{
			std::string ret = mMusicPathInfo;
			mMusicPathInfo= "";
			return ret;
		}

		return "";
	}

	std::string popTestString() 
	{ 		
		if (!mTestString.empty())
		{
			std::string ret = mTestString;
			mTestString= "";
			return ret;
		}
    
		return "";
	}
	void increaseListPosition(); 
	void decreaseListPosition(); 

	virtual ~AudioManager();

	float mMusicVolume;
	int mVideoPlaying;

    std::vector<int> randomPosList;
    bool mForceRandomization;

	static void setVideoPlaying(bool state);
	static void update(int deltaTime);

    //template<class RandomIt, class RandomFunc>
    //void random_shuffle(RandomIt first, RandomIt last, RandomFunc&& r)
    //{
    //    typename std::iterator_traits<RandomIt>::difference_type i, n;
    //    n = last - first;
    //    for (i = n-1; i > 0; --i) {
    //        std::swap(first[i], first[r(i+1)]);
    //    }
    //}
    template< class RandomIt >
    void random_shuffle( RandomIt first, RandomIt last )
    {
        typename std::iterator_traits<RandomIt>::difference_type i, n;
        n = last - first;
    	//srand(time(NULL));
        for (i = n-1; i > 0; --i) {
            using std::swap;
            swap(first[i], first[std::rand() % (i+1)]);
            // rand() % (i+1) isn't actually correct, because the generated number
            // is not uniformly distributed for most values of i. A correct implementation
            // will need to essentially reimplement C++11 std::uniform_int_distribution,
            // which is beyond the scope of this example.
        }
    }




private:
	AudioManager();

	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static std::shared_ptr<AudioManager> sInstance;
	
	
	static void onMusicFinished();

	void	findMusic(const std::string &path, std::vector<std::string>& all_matching_files);
	//void	playMusic(std::string path);
		
	std::string mCurrentSong;
	std::string mCurrentMusicPath;
	std::string mSystemName;
	std::string mCurrentThemeMusicDirectory;	

	std::string mCurrentRealThemeMusicDirectory;  //Juanjo
	std::string mPreviousRealThemeMusicDirectory; //Juanjo
	std::string mMusicPathInfo;
	std::string mTestString;
	bool		mPlayListItemIncrease;	
	bool		mPlayListItemDecrease;	
	bool		mRunningFromPlaylist;	
    int         mPlayListItemPos;

	bool		mInitialized;

	Mix_Music* mCurrentMusic;


};

#endif // ES_CORE_AUDIO_MANAGER_H
