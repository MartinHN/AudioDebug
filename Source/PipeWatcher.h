/*
  ==============================================================================

    PipWatcher.h
    Created: 11 Jan 2017 11:19:32am
    Author:  Martin Hermant

  ==============================================================================
*/

#ifndef PIPWATCHER_H_INCLUDED
#define PIPWATCHER_H_INCLUDED
#include "JuceHeader.h"

class PipeWatcher : public Timer{
public:

  PipeWatcher(const String & path):watchFolder(path){
    jassert(watchFolder.isDirectory());
    startTimer(500);
  }
  ~PipeWatcher(){

  }


  String getValidpipeName(const String & s){
    StringArray arr;
    arr.addTokens(s, "_","");
    if(arr.size()<2 || !arr[0].startsWith("juce"))return "";
    return arr[0]+"_"+arr[1];

  }

  bool pipeExists(const String & pipeName,Array<File> & files){
    for (auto & p:files){
      if(p.getFileName().startsWith(pipeName)){
        return true;
      }
    }
    return false;
  }
  void timerCallback() override{


      Array<File> curPipes;
      watchFolder.findChildFiles(curPipes,File::findFiles,false);
      for(auto & c : curPipes){
        String pipeName =getValidpipeName(c.getFileName());
        if(pipeName=="") continue;
        if(!pipeExists(pipeName,pipes)){
          listeners.call(&Listener::pipeAdded,pipeName);
          pipes.addIfNotAlreadyThere(c);
        }
      }
      for(auto & c : pipes){
        String pipeName =getValidpipeName(c.getFileName());
        if(pipeName=="") continue;
        if(!pipeExists(pipeName,curPipes)){
          listeners.call(&Listener::pipeRemoved,pipeName);
          pipes.removeAllInstancesOf(c);
        }
      }
  }

  class Listener{
  public:
    Listener(){};
    virtual ~Listener(){};
    virtual void pipeAdded(String & name) = 0;
    virtual void pipeRemoved(String & name) = 0;
  };

  ListenerList<Listener> listeners;


  Array<File> pipes;
  File watchFolder;
};



#endif  // PIPWATCHER_H_INCLUDED
