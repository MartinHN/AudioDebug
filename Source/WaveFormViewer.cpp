/*
 ==============================================================================

 WaveFormViewer.cpp
 Created: 11 Jan 2017 4:11:43pm
 Author:  Martin Hermant

 ==============================================================================
 */

#include "WaveFormViewer.h"

WaveFormViewer::WaveFormViewer():
verticalZoom(1),
envelopColour(Colours::wheat),
backColour(Colours::grey),
pixelsPerPoint(1),
samplePerPoint(100),
visibleSample(0,0),
samplesRef(nullptr),
samplePerTick(2000),
autoAdjustSamplePerPoints(true),
lastSamplePerPoint(-1),
levelsDirty(true),
bufferROI(-1,-1),
cursorPos(0),
hoverCursorX(0),
sampleRate(44100),
BPM(60),
tickPerBeat(1)


{

}
WaveFormViewer::~WaveFormViewer(){

}
void WaveFormViewer::setRefBuffer(AudioBuffer<float> & ref,int channelToLook){
  levelsDirty = true;
  levels.clear();
  samplesRef = &ref;
  channel =channelToLook ;

  setZoom  (getBufferROI());


}

bool WaveFormViewer::hasValidSampleRef(){
  return samplesRef!=nullptr;
}


Range<int> WaveFormViewer::getCurrentZoom(){
  return visibleSample;
}
void WaveFormViewer::updateLevels(int start, int num){
  if(!hasValidSampleRef())return;
  ScopedLock lk(levels.getLock());
  bool changedSize = levels.size()*samplePerPoint != getBufferROI().getLength();

  bool targetLevelIsNull = true;
  if(levels.size()){
    Range<float> testedLevel = levels[jmin(levels.size()-1,start/samplePerPoint + 1)];
    targetLevelIsNull= levels.size()>0 && testedLevel.getStart()==0 &&testedLevel.getEnd()==0;
  }
  if(changedSize || targetLevelIsNull || ( needNewSamplePerPoint())){
    levels.resize(getBufferROI().getLength()/samplePerPoint);
    int startIdx = start/samplePerPoint;
    int endIdx = num<0?levels.size():jmin(levels.size(),startIdx+num/samplePerPoint);
    if(startIdx==0 && endIdx==levels.size()){
      int dbg;
      dbg++;
    }
    for(int i = startIdx ; i < endIdx ; i++){
      levels.getReference(i) = FloatVectorOperations::findMinAndMax(samplesRef->getReadPointer(channel)+(i*samplePerPoint), samplePerPoint);
    }
    levelsDirty = false;
    generatePath();
  }


}

bool WaveFormViewer::needNewSamplePerPoint(){
  if(autoAdjustSamplePerPoints && getWidth()>0){
    int currentSamplePerPixels =(visibleSample.getLength())*1.0/getWidth();
    samplePerPoint = jmax(1,nextPowerOfTwo(currentSamplePerPixels)*2);
    if(samplePerPoint!=lastSamplePerPoint){
      lastSamplePerPoint = samplePerPoint;
      return true;
    }
  }
  return false;
}


void WaveFormViewer::setZoom(Range<int> visible,bool allowUniLateralScaling,bool notify){

  if(!hasValidSampleRef())return;
  //  DBG(visible);
  if(allowUniLateralScaling || getBufferROI().contains(visible) ){
    // allow 1/4 sample per pixel minimum
    if(  visible.getLength() > getWidth()/4){
      int newStart = jmin(getBufferROI().getEnd()-1,jmax(-100,visible.getStart()));
      int newEnd = jmax(visibleSample.getStart()+2,jmin(getBufferROI().getEnd()+0,jmax(100,visible.getEnd())));
      if(notify && (newStart!=visibleSample.getStart() || newEnd!=visibleSample.getEnd())){
        listeners.call(&Listener::positionChanged,this);
      }
      visibleSample.setStart(newStart);
      visibleSample.setEnd(newEnd);

      if(needNewSamplePerPoint()){
        updateLevels();
      }
      generatePath();
    }
  }
  else{
    //    DBG("zoom locked");
  }


}

void WaveFormViewer::setZoomMax(){
  setZoom(getBufferROI());
}
float WaveFormViewer::getNumPointsDisplayed(){
  return jmax(0.0f,(visibleSample.getLength())*1.0f/samplePerPoint);
}

void WaveFormViewer::resized(){
  if(needNewSamplePerPoint()){
    updateLevels();
  }
  generatePath();
}

void WaveFormViewer::setBPM(double _BPM){
  BPM = _BPM;
  updateTicks();
}
void WaveFormViewer::setSampleRate(float sR){
  sampleRate = sR;
  updateTicks();
}

void WaveFormViewer::updateTicks(){
  samplePerTick = sampleRate*60.0/(BPM * tickPerBeat);

}

void WaveFormViewer::setCursorPos(int sample,bool notify){
  cursorPos = sample;
  if(notify)listeners.call(&Listener::cursorChanged,this,false );
  repaint();
}

void WaveFormViewer::generatePath(){
  if(!juce::MessageManager::getInstance()->isThisTheMessageThread()){
    postCommandMessage(commands::updatePath);
    return;
  }

  path.clear();
  if(getNumPointsDisplayed()==0)return;
  {
    ScopedLock lk(levels.getLock());
    Rectangle<int > area = getLocalBounds();
    float startView = visibleSample.getStart()*1.0f/samplePerPoint;
    float endView = jmin(levels.size()-1.0f,visibleSample.getEnd()*1.0f/samplePerPoint);

    int pathOffsetX = startView<0?-startView*pixelsPerPoint:0;
    startView = jmax(0.0f,startView);
    Point<float> startPoint (area.getX()+pathOffsetX,area.getBottom()/2);
    float heightZoom = area.getHeight()*verticalZoom/2;

    pixelsPerPoint = area.getWidth()*1.0/getNumPointsDisplayed();
    path.startNewSubPath(startPoint);


    for(int i = startView ; i <= endView ; i++){
      path.lineTo(startPoint.getX() + (i-startView)*pixelsPerPoint,startPoint.getY() +1+ levels[i].getEnd()*heightZoom);
    }

    for(int i = endView; i >= startView ; i--){
      path.lineTo(startPoint.getX() +(i-startView)*pixelsPerPoint,startPoint.getY() +levels[i].getStart()*heightZoom);
    }
    path.closeSubPath();
  }
  repaint();
}

void WaveFormViewer::handleCommandMessage(int commandId){
  switch(commandId){
    case commands::updatePath:
      generatePath();
      break;
    default:
      jassertfalse;


  }
}

int WaveFormViewer::screenToSample(int x){return visibleSample.getStart() + (x - getLocalBounds().getX())*(visibleSample.getLength())/getWidth() ;}




void WaveFormViewer::mouseDown (const MouseEvent& event){
  originEndSample = visibleSample.getEnd();
  originStartSample = visibleSample.getStart();
  originDragSample =  screenToSample(event.getMouseDownX());
};

void WaveFormViewer::mouseDrag (const MouseEvent& event) {
  const float scaleDrag =event.getDistanceFromDragStartY()*1.0/getWidth() ;//(visibleSample.getEnd()-visibleSample.getStart())*0.00006*
  float horizontalFactor =jmax(0.0,(1.0 + scaleDrag));
  int horizontalDelta = screenToSample(-event.getDistanceFromDragStartX())-visibleSample.getStart();

  setZoom(Range<int>(originDragSample +horizontalDelta -  (originDragSample - originStartSample)*horizontalFactor ,
                     originDragSample +horizontalDelta +  (originEndSample- originDragSample )*horizontalFactor)
          );

};

void WaveFormViewer::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel){
  const float scaleWheel = (visibleSample.getLength())*1.0 ;//samplePerPoint*1.0/pixelsPerPoint*getWidth();
  float increment = scaleWheel*wheel.deltaY;
  float deltaX = -scaleWheel*wheel.deltaX;


  // some jubug sending negative mouse position when inertial
  if(!wheel.isInertial)lastValidWheelX = event.getPosition().x;

  float xPct  = lastValidWheelX*1.0/getWidth();//(screenToSample(lastValidWheelX) - visibleSample.getStart())*1.0/(visibleSample.getEnd() - visibleSample.getStart());
  setZoom(Range<int>(visibleSample.getStart() - xPct*increment + deltaX ,
                     visibleSample.getEnd() + (1-xPct)*increment +deltaX)
          );


};
void WaveFormViewer::mouseUp (const MouseEvent& event){
  if(!event.mouseWasDraggedSinceMouseDown()){
    setCursorPos(screenToSample(event.getMouseDownX()),true);
  }
}

void WaveFormViewer::mouseDoubleClick (const MouseEvent& event){
  if(event.getNumberOfClicks()==2){
    setZoom( getBufferROI());
  }
};

void WaveFormViewer::mouseMove(const MouseEvent& event){
  setHoverCursorPos(event.getPosition().x);

};


void WaveFormViewer::setHoverCursorPos(int pixels,bool notify){
  hoverCursorX = pixels;
  if (notify)listeners.call(&Listener::cursorChanged,this,true);
  repaint();
}

void WaveFormViewer::paint(Graphics & g){
  Rectangle < int> area = getLocalBounds();
  g.fillAll(backColour);

  g.setColour(envelopColour);
  g.fillPath(path);

  g.setColour(Colours::black);
  g.drawText(String(jmax(0,visibleSample.getStart())), area, Justification::bottomLeft);
  g.drawText(String(jmin(visibleSample.getEnd(),getBufferROI().getEnd())), area, Justification::bottomRight);

  int mouseSample = screenToSample(hoverCursorX);
  String cursors = "cursor ("+String(cursorPos)+") || "+
  "mouse ( x : "+String(mouseSample)+", y : "+String(levels[mouseSample/samplePerPoint].getStart())+ ") || "+
  "BPM : "+String(BPM);
  g.drawText(cursors, area, Justification::centredBottom);

  g.setColour(Colours::white);
  float pixelsPerTick = samplePerTick*pixelsPerPoint*1.0/samplePerPoint;
  float firstTick = visibleSample.getStart()*1.0/samplePerTick;
  if(firstTick<0)firstTick+=1;
  firstTick = pixelsPerTick *(1.0 - (firstTick - floor(firstTick)));

  float lastTick = (visibleSample.getLength())*pixelsPerPoint*1.0/samplePerPoint ;
  for(float i = firstTick ; i<lastTick  ; i+=pixelsPerTick){
    g.drawLine(i, area.getY(), i, area.getBottom(), 1);
  }

  g.drawLine(hoverCursorX, area.getY(), hoverCursorX, area.getBottom(),.1);

  g.setColour(Colours::red);
  int cursorScreenPosX = (cursorPos-visibleSample.getStart())*pixelsPerPoint/samplePerPoint;
  g.drawLine(cursorScreenPosX, area.getY(), cursorScreenPosX, area.getBottom(), 2);

}

Range<int> WaveFormViewer::getBufferROI(){
  if(!hasValidSampleRef()) return Range<int>(0,1);
  return Range<int> ( jmax(bufferROI.getStart(),0),
                     bufferROI.getEnd()<0?samplesRef->getNumSamples() : jmin(samplesRef->getNumSamples(),bufferROI.getEnd()));
  
  
}
