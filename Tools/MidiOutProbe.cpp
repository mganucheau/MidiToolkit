// Console probe: hosts the installed VST3 the way a DAW does and reports its
// MIDI capabilities and bus layout. Usage: MidiOutProbe [path-to-vst3]

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    juce::File bundle(argc > 1
        ? juce::String(argv[1])
        : juce::File::getSpecialLocation(juce::File::userHomeDirectory)
              .getChildFile("Library/Audio/Plug-Ins/VST3/Midi Toolkit.vst3")
              .getFullPathName());

    std::cout << "probing: " << bundle.getFullPathName() << "\n";

    juce::AudioPluginFormatManager fm;
    fm.addFormat(new juce::VST3PluginFormat());

    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> found;
    vst3.findAllTypesForFile(found, bundle.getFullPathName());
    if (found.isEmpty())
    {
        std::cout << "FAIL: no VST3 classes found in bundle\n";
        return 1;
    }

    for (auto* desc : found)
    {
        std::cout << "class: " << desc->name
                  << "  isInstrument=" << (desc->isInstrument ? 1 : 0) << "\n";

        juce::String err;
        auto inst = fm.createPluginInstance(*desc, 44100.0, 512, err);
        if (inst == nullptr)
        {
            std::cout << "FAIL: could not instantiate: " << err << "\n";
            return 1;
        }

        std::cout << "  acceptsMidi=" << (inst->acceptsMidi() ? 1 : 0)
                  << "  producesMidi=" << (inst->producesMidi() ? 1 : 0) << "\n";
        std::cout << "  audio buses: in=" << inst->getBusCount(true)
                  << " out=" << inst->getBusCount(false) << "\n";

        // Pump a few blocks with a fake playing transport to confirm the
        // wrapper delivers events (none expected without a clip, but this
        // proves processBlock runs in a host context).
        inst->prepareToPlay(44100.0, 512);
        juce::AudioBuffer<float> audio(juce::jmax(2, inst->getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        int events = 0;
        for (int i = 0; i < 20; ++i)
        {
            midi.clear();
            inst->processBlock(audio, midi);
            events += midi.getNumEvents();
        }
        std::cout << "  events from 20 blocks (no clip loaded): " << events << "\n";
        inst->releaseResources();
    }
    return 0;
}
