/*
  ==============================================================================

    IvoxActivityComponent.cpp
    Created: 29 May 2018 10:14:27pm
    Author:  adamson

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "IvoxActivityComponent.h"
#include "protoDebug.h"

//==============================================================================
IvoxActivityComponent::IvoxActivityComponent()
 : activity_status(DEAD), is_animated(true),
   bg_colour(Colours::grey)
{
    // This done simply for temporary array initialization
    const Colour blueShades[5]  =
    {
            Colour(0xff3838ff),
            Colour(0xff7171ff),
            Colour(0xff7171ff),
            Colour(0xffaaaaff),
            Colour (0xffe3e3ff)
    };
    memcpy(blue_shades, blueShades, sizeof(blueShades));
    // Convert blueShades[] to grayscale values
    for (int i = 0; i < 5; i++)
    {
        grey_shades[i] = Colour::greyLevel(blueShades[i].getPerceivedBrightness());
        dead_shades[i] = grey_shades[i].brighter();
    }

    setFramesPerSecond(10);
    setVisible(true);
    setSize(128, 128);
}

IvoxActivityComponent::~IvoxActivityComponent()
{
}

void IvoxActivityComponent::SetStatus(Status status, bool animate)
{
    is_animated = animate;
    if (animate && (PLAY == status))
        setFramesPerSecond(10);
    else
        setFramesPerSecond(1);
    activity_status = status;
    repaint();
}  // end IvoxActivityComponent::SetStatus()

void IvoxActivityComponent::paint(Graphics& g)
{
   // Fill background
    g.fillAll(Colour(bg_colour));
    // Draw 'speaker' circle
    // 'diameter' is min(width, height)
    float diameter = (getWidth() < getHeight() ? getWidth() : getHeight());
    float radius = diameter / 2.0;
    // 'c' is the centre point
    Point<float> c(getWidth()/2.0, getHeight()/2.0);
    // Set 'speaker' background gradient per 'activity_status'
    Colour* shade;
    if (activity_status > IDLE)
        shade = blue_shades;
    else if (activity_status > DEAD)
        shade = grey_shades;
    else
        shade = dead_shades;
    ColourGradient gradient(shade[0], c.x, c.y,
                            shade[4], c.x, c.y + radius, true);
    gradient.addColour(0.25, shade[1]);
    gradient.addColour(0.50, shade[2]);
    gradient.addColour(0.75, shade[3]);
    g.setGradientFill(gradient);
    
    g.fillEllipse (c.x - radius, c.y - radius, diameter, diameter);
    
    if (PLAY == activity_status)
    {
        g.setColour(blue_shades[0]);
        float rscale = 1.0;
        if (is_animated)
            rscale = ((float)abs(getFrameCounter() % 20 - 10)) / 10.0;
        // Draw 5 concentric circles as 'speaker' coil
        for (unsigned int i = 0; i < 5; i++)
        {
            float s = ((float)(i+1)) / 10.0;
            float r = 2.0*s*rscale*radius;
            double d = 2.0*r;
            g.drawEllipse(c.x - r, c.y - r, d, d, 1.0);
        }
    }
}  // end IvoxActivityComponent::paint()

void IvoxActivityComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}
