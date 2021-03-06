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

#pragma once

namespace foleys
{

VideoPluginManager::VideoPluginManager (VideoEngine& videoEngineToUse)
  : videoEngine (videoEngineToUse)
{
    juce::ignoreUnused (videoEngine);
    registerVideoProcessor ("BUILTIN: " + ColourCurveVideoProcessor::getPluginName(), [] { return std::make_unique<ColourCurveVideoProcessor>(); });
}

void VideoPluginManager::registerVideoProcessor (const juce::String& identifierString, std::function<std::unique_ptr<VideoProcessor>()> factory)
{
    factories [identifierString] = std::move (factory);
}

std::unique_ptr<VideoProcessor> VideoPluginManager::createVideoPluginInstance (const juce::String& identifierString,
                                                                               juce::String& error) const
{
    auto factory = factories.find (identifierString);
    if (factory != factories.cend())
    {
        error.clear();
        return factory->second();
    }

    error = NEEDS_TRANS ("Plugin not known");
    return {};
}

void VideoPluginManager::populatePluginSelection (juce::PopupMenu& menu)
{
    int i = 0;
    for (auto it : factories)
        menu.addItem (++i, it.first);
}

juce::String VideoPluginManager::getPluginDescriptionFromMenuID (int index)
{
    if (juce::isPositiveAndBelow (index - 1, factories.size()))
        return std::next (factories.begin(), index - 1)->first;

    return {};
}


} // foleys
