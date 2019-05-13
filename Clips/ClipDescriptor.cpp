/*
 ==============================================================================

 Copyright (c) 2019, Foleys Finest Audio - Daniel Walz
 All rights reserved.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 OF THE POSSIBILITY OF SUCH DAMAGE.

 ==============================================================================
 */

namespace foleys
{

namespace IDs
{
    static juce::Identifier audioProcessor  { "AudioProcessor" };
    static juce::Identifier identifier      { "Identifier" };
    static juce::Identifier name            { "Name" };
    static juce::Identifier parameter       { "Parameter" };
    static juce::Identifier value           { "Value" };
    static juce::Identifier keyframe        { "Keyframe" };
    static juce::Identifier time            { "Time" };
    static juce::Identifier pluginStatus    { "PluginStatus" };
};

ClipDescriptor::ClipDescriptor (ComposedClip& ownerToUse, std::shared_ptr<AVClip> clipToUse)
: owner (ownerToUse)
{
    clip = clipToUse;
    state = juce::ValueTree (IDs::clip);
    auto mediaFile = clip->getMediaFile();
    if (mediaFile.getFullPathName().isNotEmpty())
        state.setProperty (IDs::source, mediaFile.getFullPathName(), nullptr);

    state.getOrCreateChildWithName (IDs::videoProcessors, nullptr);
    state.getOrCreateChildWithName (IDs::audioProcessors, nullptr);

    state.addListener (this);
}

ClipDescriptor::ClipDescriptor (ComposedClip& ownerToUse, juce::ValueTree stateToUse)
: owner (ownerToUse)
{
    juce::ScopedValueSetter<bool> manual (manualStateChange, true);

    state = stateToUse;
    auto* engine = owner.getVideoEngine();
    if (state.hasProperty (IDs::source) && engine)
    {
        auto source = state.getProperty (IDs::source);
        clip = engine->createClipFromFile ({ source });

        const auto audioProcessorsNode = state.getOrCreateChildWithName (IDs::audioProcessors, nullptr);
        for (const auto& audioProcessor : audioProcessorsNode)
            addAudioProcessor (std::make_unique<AudioProcessorHolder>(*this, audioProcessor));

//        const auto videoProcessorsNode = state.getOrCreateChildWithName (IDs::videoProcessors, nullptr);
//        for (const auto& videoProcessor : videoProcessorsNode)
//            addVideoProcessor (videoProcessor);

    }
    state.addListener (this);
}

juce::String ClipDescriptor::getDescription() const
{
    return state.getProperty (IDs::description, "unnamed");
}

void ClipDescriptor::setDescription (const juce::String& name)
{
    state.setProperty (IDs::description, name, owner.getUndoManager());
}

double ClipDescriptor::getStart() const
{
    return state.getProperty (IDs::start, 0.0);
}

void ClipDescriptor::setStart (double s)
{
    state.setProperty (IDs::start, s, owner.getUndoManager());
}

double ClipDescriptor::getLength() const
{
    return state.getProperty (IDs::length, 0.0);
}

void ClipDescriptor::setLength (double l)
{
    state.setProperty (IDs::length, l, owner.getUndoManager());
}

double ClipDescriptor::getOffset() const
{
    return state.getProperty (IDs::offset, 0.0);
}

void ClipDescriptor::setOffset (double o)
{
    state.setProperty (IDs::offset, o, owner.getUndoManager());
}

int ClipDescriptor::getVideoLine() const
{
    return state.getProperty (IDs::videoLine, 0.0);
}

void ClipDescriptor::setVideoLine (int line)
{
    state.setProperty (IDs::videoLine, line, owner.getUndoManager());
}

int ClipDescriptor::getAudioLine() const
{
    return state.getProperty (IDs::audioLine, 0.0);
}

void ClipDescriptor::setAudioLine (int line)
{
    state.setProperty (IDs::audioLine, line, owner.getUndoManager());
}

void ClipDescriptor::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged,
                                                             const juce::Identifier& property)
{
    if (treeWhosePropertyHasChanged != state)
        return;

    updateSampleCounts();
}

void ClipDescriptor::valueTreeChildAdded (juce::ValueTree& parentTree,
                                          juce::ValueTree& childWhichHasBeenAdded)
{
    if (manualStateChange)
        return;

    if (parentTree.getType() == IDs::audioProcessors)
        addAudioProcessor (std::make_unique<AudioProcessorHolder>(*this, childWhichHasBeenAdded),
                           parentTree.indexOf (childWhichHasBeenAdded));
}

void ClipDescriptor::valueTreeChildRemoved (juce::ValueTree& parentTree,
                                            juce::ValueTree& childWhichHasBeenRemoved,
                                            int indexFromWhichChildWasRemoved)
{
    if (manualStateChange)
        return;

    if (parentTree.getType() == IDs::audioProcessors)
        removeAudioProcessor (indexFromWhichChildWasRemoved);
}

void ClipDescriptor::updateSampleCounts()
{
    auto sampleRate = clip->getSampleRate();

    start = sampleRate * double (state.getProperty (IDs::start));
    length = sampleRate * double (state.getProperty (IDs::length));
    offset = sampleRate * double (state.getProperty (IDs::offset));
}

int64_t ClipDescriptor::getStartInSamples() const { return start; }
int64_t ClipDescriptor::getLengthInSamples() const { return length; }
int64_t ClipDescriptor::getOffsetInSamples() const { return offset; }

juce::ValueTree& ClipDescriptor::getStatusTree()
{
    return state;
}

void ClipDescriptor::addAudioProcessor (std::unique_ptr<AudioProcessorHolder> processor, int index)
{
    auto* undo = owner.getUndoManager();

    if (processor->processor.get() != nullptr)
        processor->processor->prepareToPlay (owner.getSampleRate(), owner.getDefaultBufferSize());

    {
        juce::ScopedValueSetter<bool> manual (manualStateChange, true);
        auto processorsNode = state.getOrCreateChildWithName (IDs::audioProcessors, undo);
        processorsNode.addChild (processor->getProcessorState(), index, undo);
    }

    juce::ScopedLock sl (owner.getCallbackLock());
    if (juce::isPositiveAndBelow (index, audioProcessors.size()))
        audioProcessors.insert (std::next (audioProcessors.begin(), index), std::move (processor));
    else
        audioProcessors.push_back (std::move (processor));
}

void ClipDescriptor::addAudioProcessor (std::unique_ptr<juce::AudioProcessor> processor, int index)
{
    addAudioProcessor (std::make_unique<AudioProcessorHolder>(*this, std::move (processor)), index);
}

void ClipDescriptor::removeAudioProcessor (int index)
{
    juce::ScopedLock sl (owner.getCallbackLock());
    audioProcessors.erase (std::next (audioProcessors.begin(), index));
}

ComposedClip& ClipDescriptor::getOwningClip()
{
    return owner;
}

//==============================================================================

ClipDescriptor::AudioProcessorHolder::AudioProcessorHolder (ClipDescriptor& ownerToUse,
                                                            std::unique_ptr<juce::AudioProcessor> processorToUse)
    : owner (ownerToUse)
{
    state = juce::ValueTree { IDs::audioProcessor };
    processor = std::move (processorToUse);
    if (processor.get() != nullptr)
    {
        state.setProperty (IDs::name, processor->getName(), nullptr);

        if (auto* instance = dynamic_cast<juce::AudioPluginInstance*>(processor.get()))
        {
            auto identifier = instance->getPluginDescription().createIdentifierString();
            state.setProperty (IDs::identifier, identifier, nullptr);
        }
        else
        {
            state.setProperty (IDs::identifier, "BUILTIN: " + processor->getName(), nullptr);
        }

        for (auto parameter : processor->getParameters())
            if (parameter->isAutomatable())
                parameters.push_back (std::make_unique<AutomationParameter> (*processor, *parameter));
    }

    for (auto& parameter : parameters)
    {
        juce::ValueTree automation { IDs::parameter };
        automation.setProperty (IDs::name, parameter->getName(), nullptr);
        automation.setProperty (IDs::value, parameter->getValue(), nullptr);

        for (const auto& key : parameter->getKeyframes())
        {
            juce::ValueTree keyframeNode { IDs::keyframe };
            keyframeNode.setProperty (IDs::time, key.first, nullptr);
            keyframeNode.setProperty (IDs::value, key.second, nullptr);
            automation.appendChild ( keyframeNode, nullptr);
        }

        state.appendChild (automation, nullptr);
    }
}

ClipDescriptor::AudioProcessorHolder::AudioProcessorHolder (ClipDescriptor& ownerToUse,
                                                            const juce::ValueTree& stateToUse, int index)
    : owner (ownerToUse)
{
    state = stateToUse;

    const auto identifier = state.getProperty (IDs::identifier);
    auto& composedClip = owner.getOwningClip();
    auto* engine = composedClip.getVideoEngine();

    if (engine == nullptr)
    {
        state.setProperty (IDs::pluginStatus, NEEDS_TRANS ("Video engine not present"), nullptr);
        return;
    }

    juce::String error;
    processor = engine->createAudioPluginInstance (identifier, composedClip.getSampleRate(), composedClip.getDefaultBufferSize(), error);

    state.setProperty (IDs::pluginStatus, error, nullptr);

    if (processor.get() == nullptr)
        return;

    for (auto* parameter : processor->getParameters())
    {
        if (!parameter->isAutomatable())
            continue;

        parameters.emplace_back (std::make_unique<AutomationParameter>(*processor, *parameter));
//        auto node = state.getChildWithProperty (IDs::name, parameter->getName());
//        if (node.isValid())
//        {
//
//        }
    }
}

ClipDescriptor::AudioProcessorHolder::~AudioProcessorHolder()
{
    if (processor.get() != nullptr)
        processor->releaseResources();
}

void ClipDescriptor::AudioProcessorHolder::updateAutomation (double pts)
{
    for (auto& parameter : parameters)
        parameter->updateProcessor (pts);
}

juce::ValueTree ClipDescriptor::AudioProcessorHolder::getProcessorState() const
{
    return state;
}

} // foleys