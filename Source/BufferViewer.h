/*
 ==============================================================================

 BufferViewer.h
 Created: 11 Jan 2017 11:19:13am
 Author:  Martin Hermant

 ==============================================================================
 */

#ifndef BUFFERVIEWER_H_INCLUDED
#define BUFFERVIEWER_H_INCLUDED

#include "JuceHeader.h"
#include "WaveFormViewer.h"

class BufferViewer : public Component,Button::Listener,Thread,AsyncUpdater{
public:
  BufferViewer(const String & path):Thread("Pipe : "+getCanonicalName(path)){

    blockSize = 4096;
    buffer.setSize(1,4*blockSize);
    buffer.clear();
    openForPath(path);
    startThread();
    enveloppe.setRefBuffer(buffer);

    addAndMakeVisible(enveloppe);
    clearB.setClickingTogglesState(true);
    clearB.setButtonText("auto clear");
    clearB.setToggleState(true, dontSendNotification);
    addAndMakeVisible(clearB);
    clearB.addListener(this);



  }

  ~BufferViewer(){

    stopThread(1000);
  }
  void openForPath(const String & path){
    if(canonicalName==""){
      canonicalName = getCanonicalName(path);
    }
    int numChannel = channelFromPath(path)+1;
    ScopedLock lk(inPipes.getLock());
    int curSize = inPipes.size();
    jassert(numChannel>=curSize);
    for(int i = curSize ; i < numChannel ; i++){
      inPipes.add(new NamedPipe());//jmax(numChannel,inPipes.size()));
      jassert(inPipes.getLast()->openExisting(canonicalName+String(i)));

    }
    writeNeedles.resize(numChannel);

  }
  int channelFromPath(const String & path){
    if(isdigit(path[path.length() - 1])){return path[path.length() - 1]-48;}
    else{return 0;}

  }
 static  String getCanonicalName(const String & path ){
    if(isdigit(path[path.length() - 1])){return path.substring(0, path.length()-1);}
    else{return path;}

  }
  template<typename T>
  int tryReadBlockSize(NamedPipe & pipe,T * dest,int bS){
    if(!pipe.isOpen()){
      return -1;
    }
    int idx = 0;
    while(idx<bS ){

      int read = pipe.read(dest+idx, 1*sizeof(T), 30);
      if(threadShouldExit())return -1;
      if(read<0){
        if(idx==0)
          return -1;
        else
          return idx;
        //        jassertfalse;
        //        break;
      }
      else if (read==0){
        jassertfalse;
        break;
      };
      idx++;
    }

    return idx;
  }
  void run() override{
    while (!threadShouldExit()){
      ScopedLock lk(inPipes.getLock());
      if(getLastWriteNeedle()+blockSize>buffer.getNumSamples()){
        buffer.setSize(buffer.getNumChannels(), buffer.getNumSamples()+blockSize,true,true);
      }

      int idx = 0;
      for(auto &inPipe : inPipes){

        if(inPipes.size()>buffer.getNumChannels()){
          buffer.setSize(inPipes.size(),buffer.getNumSamples(),true,true);
          enveloppe.setRefBuffer(buffer);
        }
        int read = tryReadBlockSize<float>(*inPipe,buffer.getWritePointer(idx)+writeNeedles[idx], blockSize); //inPipe.read(buffer.getWritePointer(0)+writeNeedle, blockSize*sizeof(float), -1);
        int numSamples = read;
        if(numSamples <= 1 ){
          if(writeNeedles[idx]>0){
            
            triggerAsyncUpdate();
            if(clearB.getToggleState())isOld=true;
          }
          writeNeedles.getReference(idx) = 0;
        }
        if(numSamples>0){

          if(isOld){
            for(int i = 0 ; i < numSamples ; i++){
              buffer.setSample(idx, i, buffer.getSample(0,writeNeedles[idx]+i));
            }
            writeNeedles.getReference(idx) =0;
            isOld = false;
          }

          enveloppe.updateLevels(writeNeedles[idx],numSamples);
          writeNeedles.getReference(idx)+=numSamples;
          //        DBG("read : " << numSamples << "," << writeNeedle+1);



          triggerAsyncUpdate();
        }
        idx++;
      }

      if(msgPipe.isOpen()){
        int msgRead =tryReadBlockSize<char>(msgPipe,&msg[0], juce::numElementsInArray(msg)); //
        if(msgRead>=0)processMessage(String::fromUTF8(&msg[0]));
      }

    }
  }

  int getLastWriteNeedle(){
    int res=0;
    for(auto & w:writeNeedles){
      res = jmax(w,res);
    }
    return res;
  }
  void processMessage(const String & msg){
    DBG(msg);
    StringArray arr;
    arr.addTokens(msg," ","");
    if(arr.size()){
      String cmd = arr[0];
      arr.remove(0);
      if(cmd=="BPM"){
        enveloppe.setBPM(arr[0].getFloatValue());
      }
      else if(cmd=="SampleRate"){
        enveloppe.setSampleRate(arr[0].getFloatValue());
      }

    }
    else{
      jassertfalse;
    }
  }
  void handleAsyncUpdate()override{
//    enveloppe.setZoomMax();
  }

  void buttonClicked(Button * b) override{

  }

  void paintOverChildren(Graphics & g) override{
    g.setColour(Colours::white);
    g.drawText(canonicalName+"_"+String(enveloppe.getNumChannels()), getLocalBounds().removeFromLeft(300), Justification::centredLeft);

  }

  void paint(Graphics & g) override{
    g.fillAll(Colours::black);
  }
  void resized()override{

    Rectangle<int> area = getLocalBounds();
    Rectangle<int> ctlPanel = area.removeFromTop(30);
    clearB.setBounds(ctlPanel);
    enveloppe.setBounds(area.reduced(10));
  }



  OwnedArray<NamedPipe,CriticalSection> inPipes;
  NamedPipe msgPipe;
  WaveFormViewer enveloppe;
  String canonicalName;
  Array<int> writeNeedles;
  int lastUpdatedWrite;

  int blockSize ;
  char  msg[2048];

  TextButton clearB;
  bool isOld;

  AudioBuffer<float> buffer;

};

class BufferList :  public Component,WaveFormViewer::Listener{

public:
  BufferList(){
    linkTimeLines.setButtonText("linkTimeLines");
    linkTimeLines.setClickingTogglesState(true);
    linkTimeLines.setToggleState(true,dontSendNotification);
    addAndMakeVisible(linkTimeLines);

  };

  void addBuffer(const String & path){
    bool isMsgPath = path.startsWith("juceMsg");
    if(isMsgPath){
      StringArray arr;
      arr.addTokens(path,"_","");
      String audioBufName = "juce_"+arr[1];
      int idx = idxOfBuffer(audioBufName);
      if(idx>=0){
        jassert(buffers[idx]->msgPipe.openExisting(path));
      }
    }
    else{
      int idx = idxOfBuffer(path);
      if(idx< 0){
        BufferViewer * b=new BufferViewer(path);
        b->enveloppe.listeners.add(this);
        addAndMakeVisible(b);
        buffers.add(b);
      }
      else{
        buffers[idx]->openForPath(path);
      }
      resized();
    }
  }

  void removeBuffer(const String & path){
    int idx = idxOfBuffer(path);
    if(idx>= 0){
      buffers[idx]->enveloppe.listeners.remove(this);
      removeChildComponent(buffers[idx]);

      buffers.remove(idx);
    }
    resized();
  }

  int idxOfBuffer(const String & p){
    int i = 0;
    String can = BufferViewer::getCanonicalName(p);
    for ( auto & b:buffers){
      if(b->canonicalName==can){
        return i;
      }
      i++;
    }
    return -1;
  }

  void resized() override{
    if(buffers.size()==0) return;
    Rectangle <int > area = getLocalBounds();
    Rectangle<int > ctlPanel = area.removeFromTop(40);
    linkTimeLines.setBounds(ctlPanel);
    int step = area.getHeight()/buffers.size();
    for(auto & b:buffers){
      b->setBounds(area.removeFromTop(step));

    }
  }

  void positionChanged(WaveFormViewer * o)override{
    if(linkTimeLines.getToggleState()){
      Range<int> cZ = o->getCurrentZoom();

      for(auto & b:buffers){
        if(&b->enveloppe!=o){
          double ratio = o->BPM/b->enveloppe.BPM;

          b->enveloppe.setZoom(Range<int>(cZ.getStart()*ratio,cZ.getEnd()*ratio),true,false);
        }
      }
    }
  }

  void cursorChanged(WaveFormViewer * o, bool isHover)override{
    if(linkTimeLines.getToggleState()){
      if(isHover){
        float pos =o->hoverCursorX;
        for(auto & b:buffers){
          if(&b->enveloppe!=o){
            double ratio = o->BPM/b->enveloppe.BPM;
            b->enveloppe.setHoverCursorPos(pos*ratio,false);
          }

        }

      }
      else{
        float pos =o->cursorPos;
        for(auto & b:buffers){

          if(&b->enveloppe!=o){
            double ratio = o->BPM/b->enveloppe.BPM;
            b->enveloppe.setCursorPos(pos*ratio,false);
          }
        }
        
      }
    }
  }
  
  TextButton linkTimeLines;
  
  OwnedArray<BufferViewer> buffers;
};



#endif  // BUFFERVIEWER_H_INCLUDED
