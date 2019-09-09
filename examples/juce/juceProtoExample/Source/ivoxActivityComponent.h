/*
  ==============================================================================

    IvoxActivityComponent.h
    Created: 29 May 2018 10:14:27pm
    Author:  adamson

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================
/*
*/
class IvoxActivityComponent : public AnimatedAppComponent
{
    public:
        IvoxActivityComponent();
        ~IvoxActivityComponent();

        enum Status
        {
            DEAD = 0,
            IDLE,
            RECV,
            PLAY
        };
            
        void SetBackgroundColour(Colour colour)
        {
            bg_colour = colour;
            repaint();
        }
    
        Status GetStatus() const
            {return activity_status;}
        
        void SetStatus(Status status, bool animate = true); 
    
    private:
        void paint (Graphics&) override;
        void resized() override;

        void update() override {}

        Status    activity_status;
        bool      is_animated;
        Colour    bg_colour;
        Colour    blue_shades[5];
        Colour    grey_shades[5];
        Colour    dead_shades[5];
    
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IvoxActivityComponent)
                
};  // end IvoxActivityComponent
