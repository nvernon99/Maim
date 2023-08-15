/*
  ==============================================================================

    MP3ControllerManager.cpp
    Created: 10 Apr 2023 5:05:43pm
    Author:  Arden Butterfield

  ==============================================================================
*/

#include "MP3ControllerManager.h"

MP3ControllerManager::MP3ControllerManager(juce::AudioProcessorValueTreeState& p) :
    parameters(p),
    currentEncoder(lame),
    currentControllerIndex(0)
{
    parametersNeedUpdating = false;
    
    parameters.addParameterListener("butterflystandard", this);
    parameters.addParameterListener("butterflycrossed", this);
    parameters.addParameterListener("mdctstep", this);
    parameters.addParameterListener("mdctinvert", this);
    parameters.addParameterListener("mdctposthshift", this);
    parameters.addParameterListener("mdctpostvshift", this);
    parameters.addParameterListener("mdctwindowincr", this);
    parameters.addParameterListener("mdctsampincr", this);
    parameters.addParameterListener("bitrate", this);
    parameters.addParameterListener("bitratesquish", this);
    parameters.addParameterListener("thresholdbias", this);
    parameters.addParameterListener("mdctfeedback", this);
    parameters.addParameterListener("encoder", this);
    
    for (int i = 0; i < NUM_REASSIGNMENT_BANDS; ++i) {
        std::stringstream id;
        id << "bandorder" << i;
        parameters.addParameterListener(id.str(), this);
        bandReassignmentParameters[i] = (juce::AudioParameterInt*)parameters.getParameter(id.str());
    }
}

void MP3ControllerManager::initialize (int _samplerate, int _initialBitrate, int _samplesPerBlock)
{
    samplerate = _samplerate;
    samplesPerBlock = _samplesPerBlock;

    // useful for debugging
    lameControllers[0].name = "lame0";
    lameControllers[1].name = "lame1";
    bladeControllers[0].name = "blade0";
    bladeControllers[1].name = "blade1";

    if (currentEncoder == lame) {
        currentController = &(lameControllers[currentControllerIndex]);
    } else {
        currentController = &(bladeControllers[currentControllerIndex]);
    }
    currentController->init(samplerate, samplesPerBlock, _initialBitrate);
    currentController->setOutputBufferToSilence(MP3FRAMESIZE);
    offController = nullptr;

    currentBitrate = _initialBitrate;
    wantingToSwitch = false;
    switchCountdown = 0;

    startTimerHz(30);
}

MP3ControllerManager::~MP3ControllerManager()
{
    parameters.removeParameterListener("butterflystandard", this);
    parameters.removeParameterListener("butterflycrossed", this);
    parameters.removeParameterListener("mdctstep", this);
    parameters.removeParameterListener("mdctinvert", this);
    parameters.removeParameterListener("mdctposthshift", this);
    parameters.removeParameterListener("mdctpostvshift", this);
    parameters.removeParameterListener("mdctwindowincr", this);
    parameters.removeParameterListener("mdctsampincr", this);
    parameters.removeParameterListener("bitrate", this);
    parameters.removeParameterListener("bitratesquish", this);
    parameters.removeParameterListener("thresholdbias", this);
    parameters.removeParameterListener("mdctfeedback", this);
    parameters.removeParameterListener("encoder", this);
    
    for (int i = 0; i < NUM_REASSIGNMENT_BANDS; ++i) {
        std::stringstream id;
        id << "bandorder" << i;
        parameters.removeParameterListener(id.str(), this);
    }
}

void MP3ControllerManager::parameterChanged (const juce::String &parameterID, float newValue)
{
    parametersNeedUpdating = true;
}

void MP3ControllerManager::changeController(int bitrate, Encoder encoder)
{
    if ((bitrate == currentBitrate) && (encoder == currentEncoder)) {
        wantingToSwitch = false;
        offController = nullptr;
        return;
    }
    if (wantingToSwitch && (bitrate == desiredBitrate) && (encoder == desiredEncoder)) {
        return;
    }
    desiredBitrate = bitrate;
    
    int offIndex = (currentControllerIndex + 1) % 2;
    
    if (encoder == lame) {
        desiredEncoder = lame;
        offController = &(lameControllers[offIndex]);
    } else {
        desiredEncoder = blade;
        offController = &(bladeControllers[offIndex]);
    }
    
    offController->init(samplerate, samplesPerBlock, desiredBitrate);
    wantingToSwitch = true;

}

void MP3ControllerManager::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (parametersNeedUpdating) {
        updateParameters();
    }
    
    if (buffer.getNumChannels() != 2) {
        return;
    }
    auto samplesL = buffer.getWritePointer(0);
    auto samplesR = buffer.getWritePointer(1);

    for (int start = 0; start < buffer.getNumSamples(); start += samplesPerBlock) {
        int length = std::min(buffer.getNumSamples() - start, samplesPerBlock);
        currentController->addNextInput(samplesL, samplesR, length);
        if (wantingToSwitch)
        {
            offController->addNextInput (samplesL, samplesR, length);
            auto extraSamples = offController->samplesInOutputQueue() - currentController->samplesInOutputQueue();
            if (extraSamples >= 0)
            {
                offController->copyOutput (nullptr, nullptr, extraSamples);
                auto tempBuffer = juce::AudioBuffer<float> (buffer.getNumChannels(),
                    length);
                auto tempL = tempBuffer.getWritePointer (0);
                auto tempR = tempBuffer.getWritePointer (1);
                currentController->copyOutput (tempL, tempR, length);
                offController->copyOutput (samplesL, samplesR, length);
                buffer.applyGainRamp (start, length, 0, 1);
                buffer.addFromWithRamp (0, start, tempL, length, 1, 0);
                buffer.addFromWithRamp (1, start, tempR, length, 1, 0);
                currentControllerIndex = (currentControllerIndex + 1) % 2;
                currentController = offController;
                currentBitrate = desiredBitrate;
                currentEncoder = desiredEncoder;
                offController = nullptr;
                wantingToSwitch = false;
                continue;
            }
        }
        if (!currentController->copyOutput(samplesL, samplesR, length)) {
            memset(samplesL, 0, sizeof(float) * length);
            memset(samplesR, 0, sizeof(float) * length);
        }

        samplesL += samplesPerBlock;
        samplesR += samplesPerBlock;
    }
}

void MP3ControllerManager::updateParameters()
{
    auto encoder = (Encoder)((juce::AudioParameterChoice*)
                                parameters.getParameter("encoder"))->getIndex();
    int bitrate = bitrates[((juce::AudioParameterChoice*)
                            parameters.getParameter("bitrate"))->getIndex()];
    changeController(bitrate, encoder);

    for (auto controller : {offController, currentController}) {
        if (controller == nullptr) {
            continue;
        }
        controller->setButterflyBends(
            ((juce::AudioParameterFloat*) parameters.getParameter("butterflystandard"))->get(),
            ((juce::AudioParameterFloat*) parameters.getParameter("butterflycrossed"))->get(),
            ((juce::AudioParameterFloat*) parameters.getParameter("butterflycrossed"))->get(),
            ((juce::AudioParameterFloat*) parameters.getParameter("butterflystandard"))->get()
        );

        controller->setMDCTbandstepBends(
            ((juce::AudioParameterBool*) parameters.getParameter("mdctinvert"))->get(),
            ((juce::AudioParameterInt*) parameters.getParameter("mdctstep"))->get()
        );

        controller->setMDCTfeedback(
            ((juce::AudioParameterFloat*) parameters.getParameter("mdctfeedback"))->get()
        );

        controller->setMDCTpostshiftBends(
            ((juce::AudioParameterInt*) parameters.getParameter("mdctposthshift"))->get(),
           ((juce::AudioParameterFloat*) parameters.getParameter("mdctpostvshift"))->get()
        );
        controller->setMDCTwindowincrBends(
            ((juce::AudioParameterInt*) parameters.getParameter("mdctwindowincr"))->get()
        );
        controller->setBitrateSquishBends(
            ((juce::AudioParameterFloat*) parameters.getParameter("bitratesquish"))->get()
        );

         controller->setThresholdBias(((juce::AudioParameterFloat*) parameters.getParameter("thresholdbias"))->get()
        );

        int bandReassign[32];
        int i;
        for (i = 0; i < 20; ++i) {
            bandReassign[i] = bandReassignmentParameters[i]->get();
        }
        for (; i < 32; ++i) {
            bandReassign[i] = i;
        }
        controller->setMDCTBandReassignmentBends(bandReassign);
    }

    auto psychoanalState = parameters.state.getChildWithName("psychoanal");
    auto indicator = psychoanalState.getProperty("shortblockindicator");
    bool shortBlockStatus = currentController->getShortBlockStatus();
    if (!(indicator.isBool() && ((bool)indicator == shortBlockStatus))) {
        psychoanalState.setProperty("shortblockindicator", shortBlockStatus, nullptr);
    }


    
    parametersNeedUpdating = false;
}

int MP3ControllerManager::getBitrate()
{
    return currentBitrate;
}

float* MP3ControllerManager::getPsychoanalEnergy()
{
    return currentController->getPsychoanalEnergy();
}

float* MP3ControllerManager::getPsychoanalThreshold()
{
    return currentController->getPsychoanalThreshold();
}

float* MP3ControllerManager::getMDCTpreBend()
{
    return currentController->getMDCTpreBend();
}

float* MP3ControllerManager::getMDCTpostBend()
{
    return currentController->getMDCTpostBend();
}

float rescalePsychoanal(const float a) {
    return log10(a > 1 ? a : 1) / 14;
}

float rescaleMDCT(const float a) {
    // mdct is 0 to 1(+), rescale to 0 to 1 but log scale
    if (a < (pow(10.f, -10.f))) {
        return 0;
    } else if (a > 1) {
        return 1;
    } else {
        return log10(a) / 10 + 1;
    }
}

void MP3ControllerManager::timerCallback()
{
    float* energy = getPsychoanalEnergy();
    float* threshold = getPsychoanalThreshold();
    
    juce::var thresholdV, energyV;
    
    for (int i = 0; i < 22; ++i) {
        thresholdV.append(rescalePsychoanal(threshold[i]));
        energyV.append(rescalePsychoanal(energy[i]));
    }
    
    auto psychoSpectrum = parameters.state.getChildWithName("psychoanal");
    psychoSpectrum.setProperty("threshold", thresholdV, nullptr);
    psychoSpectrum.setProperty("energy", energyV, nullptr);

    float* preBend = getMDCTpreBend();
    float* postBend = getMDCTpostBend();

    juce::var preBendV, postBendV;

    for (int i = 0; i < 576; ++i) {
        preBendV.append(rescaleMDCT(preBend[i]));
        postBendV.append(rescaleMDCT(postBend[i]));
    }
    auto mdctSamples = parameters.state.getChildWithName("mdct");
    mdctSamples.setProperty("pre", preBendV, nullptr);
    mdctSamples.setProperty("post",postBendV, nullptr);
}
