/*
 ==============================================================================

 WaveFormViewer.h
 Created: 11 Jan 2017 4:11:43pm
 Author:  Martin Hermant

 ==============================================================================
 */

#ifndef WAVEFORMVIEWER_H_INCLUDED
#define WAVEFORMVIEWER_H_INCLUDED

#include "JuceHeader.h"



class WaveFormViewer : public Component{
public:
  WaveFormViewer();
  ~WaveFormViewer();

  void setRefBuffer(AudioBuffer<float> & ref,int channelToLook);
  void paint(Graphics & g)override;
  void updateLevels(int start = 0 , int end = -1);
  void setZoom(Range<int> zoom,bool allowUniLateralScaling=true,bool notify = true);
  void setZoomMax();

  void setBPM(double BPM);
  void setSampleRate(float sR);
  Range<int> getCurrentZoom();

  Colour envelopColour,backColour,tickColour;

  void resized() override;


  void setCursorPos(int sample,bool notify=true);
  void setHoverCursorPos(int pixels,bool notify=true);

  bool hasValidSampleRef();

  class Listener{
  public:
    virtual ~Listener(){};
    virtual void positionChanged(WaveFormViewer * ){};
    virtual void cursorChanged(WaveFormViewer *,bool isHover){};
  };

  ListenerList<Listener> listeners;


  Range<int> bufferROI;

  int screenToSample(int x);

  bool autoAdjustSamplePerPoints;

  int cursorPos;
  int hoverCursorX;

  double BPM;

protected:

  void generatePath();
  float getNumPointsDisplayed();
  Range<int> visibleSample;
  

  int samplePerPoint;
  float pixelsPerPoint;

  float sampleRate;
  int tickPerBeat;
  void updateTicks();
  int channel;
  float verticalZoom;
  int originStartSample,originEndSample;
  int originDragSample ;
  int samplePerTick;

  void mouseMove(const MouseEvent& event)override;
  void mouseDown (const MouseEvent& event)override;
  void mouseDrag (const MouseEvent& event) override;
  void mouseUp (const MouseEvent& event)override;
  void mouseDoubleClick (const MouseEvent& event)override;
  void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel)override;
  int lastValidWheelX;

  Range<int> getBufferROI();


  
  float lastSamplePerPoint;
  bool needNewSamplePerPoint();

  bool levelsDirty;


  enum commands{
    updatePath = 0
  };


private:
  void handleCommandMessage(int commandId)override;

  Array<Range<float>,CriticalSection > levels;

  Path path;

  AudioBuffer<float> * samplesRef;
};


#endif  // WAVEFORMVIEWER_H_INCLUDED
