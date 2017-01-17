/*
  ==============================================================================

    PhantomBuffer.h
    Created: 13 Jan 2017 12:46:07pm
    Author:  Martin Hermant

  ==============================================================================
*/

#ifndef PHANTOMBUFFER_H_INCLUDED
#define PHANTOMBUFFER_H_INCLUDED


/*
 helper class for phantom buffer
 allowing having constant access to contiguous memory in a circular buffer without dealing with overlaps
 it allocate twice the size though ...
 */
class PhantomBuffer{

public:
  PhantomBuffer(int _channels,int size):numChannels(_channels){
    phantomSize = (int)ceil(size);
    buf.setSize(numChannels,2*phantomSize,false,true);
    writeNeedle = 0;
  }

  void setNumChannels(int channels){
    numChannels= channels;
    buf.setSize(numChannels,2*phantomSize,false,true);
  }
  void writeBlock(AudioSampleBuffer & newBuf){


    jassert(newBuf.getNumChannels()>=numChannels);
    int toCopy = newBuf.getNumSamples();

    // overlap
    if( writeNeedle + toCopy > phantomSize){
      int firstSeg = phantomSize-(writeNeedle) ;
      jassert(firstSeg<newBuf.getNumSamples());
      for(int i = numChannels-1;i>=0 ;--i){
        safeCopy(newBuf.getReadPointer(i,0),firstSeg,i);
      }
      jassert(writeNeedle == 0);
      for(int i = numChannels-1;i>=0 ;--i){
        safeCopy(newBuf.getReadPointer(i,firstSeg),toCopy-firstSeg,i);
      }
      writeNeedle+=toCopy-firstSeg;
      writeNeedle%=phantomSize;
    }
    else{
      for(int i = numChannels-1;i>=0 ;--i){
        safeCopy(newBuf.getReadPointer(i),toCopy,i);
      }
      writeNeedle+=toCopy;
      writeNeedle%=phantomSize;
    }
  }


  const AudioBuffer<float> & getLastBlock(int num){
    jassert(num<=phantomSize);
    pointers.ensureStorageAllocated(numChannels);
    for(int i = 0 ; i < numChannels ; i++){
      pointers.set(i,buf.getArrayOfWritePointers()[i] + phantomSize+ writeNeedle-num);
    }
    contiguousBuffer.setDataToReferTo(pointers.getRawDataPointer(), numChannels, num);
    return contiguousBuffer;
  }

  const void printContent(){
    String niceOutput;
    for(int i = 0 ; i < buf.getNumSamples() ; i++){
      for(int c = 0 ;c < numChannels ; c++){niceOutput+="\t  "+String(buf.getSample(c,i));}
      if(i== phantomSize+ writeNeedle){niceOutput+="\t  writeNeedle";}
      if(i%phantomSize==0){niceOutput+="\t  phantomLimit";}
      niceOutput+="\n";
    }
    DBG(niceOutput);
  }
  


  AudioSampleBuffer buf;
private:
  void safeCopy(const float * b,int numSample,int channel){

    jassert(writeNeedle+numSample <= phantomSize);
    buf.copyFrom(channel, phantomSize+writeNeedle,b,numSample);
    buf.copyFrom(channel, writeNeedle,b,numSample);

  }
  int writeNeedle;
  int phantomSize;
  int numChannels ;
  AudioBuffer<float> contiguousBuffer;
  Array<float*> pointers ;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomBuffer);


};




#endif  // PHANTOMBUFFER_H_INCLUDED
