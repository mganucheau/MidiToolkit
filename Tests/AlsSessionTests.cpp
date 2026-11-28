#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "AlsSession.h"
#include "TestHelpers.h"

using namespace pflow;
using Catch::Approx;

TEST_CASE("Loading document path uses the last als in the log", "[AlsSession]")
{
    const auto log =
        "info: Loading document \"/tmp/Old.als\"\n"
        "info: Loading document \"/tmp/notes.txt\"\n"
        "info: Loading document \"/tmp/Live 12.4.6/BaseFiles/Recovered.als\"\n"
        "info: something else\n";

    const auto path = liveSetPathFromLog(log);
    REQUIRE(path.getFullPathName() == juce::File("/tmp/Live 12.4.6/BaseFiles/Recovered.als").getFullPathName());
    REQUIRE(liveSetPathFromLog("no document here").getFullPathName().isEmpty());
}

TEST_CASE("als session parser reads one gzipped clip", "[AlsSession]")
{
    const auto xml = juce::String(
        R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Ableton>
  <LiveSet>
    <Tracks>
      <GroupTrack Id="1">
        <Name>
          <EffectiveName Value="Group"/>
          <UserName Value=""/>
        </Name>
        <DeviceChain>
          <MainSequencer>
            <ClipTimeable>
              <ArrangerAutomation>
                <Events>
                  <MidiTrack Id="2">
                    <Name>
                      <EffectiveName Value="MIDI"/>
                      <UserName Value="Bass"/>
                    </Name>
                    <DeviceChain>
                      <MainSequencer>
                        <ClipTimeable>
                          <ArrangerAutomation>
                            <Events>
                              <MidiClip Id="3" Time="0">
                                <Loop>
                                  <LoopStart Value="4"/>
                                  <LoopEnd Value="8"/>
                                </Loop>
                                <Name Value="Line"/>
                                <Disabled Value="false"/>
                                <TimeSignature>
                                  <TimeSignatures>
                                    <RemoteableTimeSignature Id="0">
                                      <Numerator Value="4"/>
                                      <Denominator Value="4"/>
                                    </RemoteableTimeSignature>
                                  </TimeSignatures>
                                </TimeSignature>
                                <Notes>
                                  <KeyTracks>
                                    <KeyTrack Id="0">
                                      <Notes>
                                        <MidiNoteEvent Time="1" Duration="0.5" Velocity="10"/>
                                        <MidiNoteEvent Time="4" Duration="1" Velocity="100"/>
                                        <MidiNoteEvent Time="5.5" Duration="0.25" Velocity="80"/>
                                        <MidiNoteEvent Time="8" Duration="1" Velocity="70"/>
                                      </Notes>
                                      <MidiKey Value="36"/>
                                    </KeyTrack>
                                  </KeyTracks>
                                </Notes>
                              </MidiClip>
                              <MidiClip Id="4" Time="16">
                                <Loop>
                                  <LoopStart Value="0"/>
                                  <LoopEnd Value="4"/>
                                </Loop>
                                <Name Value="Off"/>
                                <Disabled Value="true"/>
                                <Notes>
                                  <KeyTracks>
                                    <KeyTrack Id="1">
                                      <Notes>
                                        <MidiNoteEvent Time="0" Duration="1" Velocity="100"/>
                                      </Notes>
                                      <MidiKey Value="40"/>
                                    </KeyTrack>
                                  </KeyTracks>
                                </Notes>
                              </MidiClip>
                            </Events>
                          </ArrangerAutomation>
                        </ClipTimeable>
                      </MainSequencer>
                    </DeviceChain>
                  </MidiTrack>
                </Events>
              </ArrangerAutomation>
            </ClipTimeable>
          </MainSequencer>
        </DeviceChain>
      </GroupTrack>
    </Tracks>
    <MasterTrack>
      <DeviceChain>
        <Mixer>
          <Tempo>
            <Manual Value="128"/>
          </Tempo>
        </Mixer>
      </DeviceChain>
    </MasterTrack>
  </LiveSet>
</Ableton>
)xml");

    juce::MemoryOutputStream compressed;
    {
        juce::GZIPCompressorOutputStream gzip(compressed, 9, 15 + 16);
        gzip.write(xml.toRawUTF8(), xml.getNumBytesAsUTF8());
    }

    const auto file = test::tempMidiFile("session.als");
    file.getParentDirectory().createDirectory();
    file.replaceWithData(compressed.getData(), compressed.getDataSize());

    const auto session = parseAlsSession(file);
    test::removeTempMidiFile(file);

    REQUIRE(session.setName == "session");
    REQUIRE(session.bpm == Approx(128.0));
    REQUIRE(session.clips.size() == 1);
    const auto& clip = session.clips[0];
    REQUIRE(clip.track == "Bass");
    REQUIRE(clip.name == "Line");
    REQUIRE(clip.timeSigNum == 4);
    REQUIRE(clip.timeSigDen == 4);
    REQUIRE(clip.lengthBeats == Approx(4.0));
    REQUIRE(clip.notes.size() == 2);
    REQUIRE(clip.notes[0].noteNumber == 36);
    REQUIRE(clip.notes[0].velocity == 100);
    REQUIRE(clip.notes[0].startBeat == Approx(0.0));
    REQUIRE(clip.notes[0].lengthBeats == Approx(1.0));
    REQUIRE(clip.notes[1].noteNumber == 36);
    REQUIRE(clip.notes[1].velocity == 80);
    REQUIRE(clip.notes[1].startBeat == Approx(1.5));
    REQUIRE(clip.notes[1].lengthBeats == Approx(0.25));
}
