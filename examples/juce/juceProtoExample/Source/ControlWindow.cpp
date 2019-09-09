/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 5.3.2

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2017 - ROLI Ltd.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "protoDebug.h"
//[/Headers]

#include "ControlWindow.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
ControlWindow::ControlWindow ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    startButton.reset (new ToggleButton ("startButton"));
    addAndMakeVisible (startButton.get());
    startButton->setTooltip (TRANS("start stop audio loop"));
    startButton->setButtonText (TRANS("Do Something"));
    startButton->addListener (this);
    startButton->setColour (ToggleButton::textColourId, Colours::white);

    helloLabel.reset (new Label ("helloLabel",
                                 TRANS("Hello, ProtoWorld")));
    addAndMakeVisible (helloLabel.get());
    helloLabel->setFont (Font (18.00f, Font::plain).withTypefaceStyle ("Regular"));
    helloLabel->setJustificationType (Justification::centred);
    helloLabel->setEditable (false, false, false);
    helloLabel->setColour (Label::backgroundColourId, Colours::white);
    helloLabel->setColour (Label::textColourId, Colour (0xff1e2d75));
    helloLabel->setColour (TextEditor::textColourId, Colours::black);
    helloLabel->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    viewport.reset (new Viewport ("viewport"));
    addAndMakeVisible (viewport.get());

    drawable1.reset (Drawable::createFromImageData (BinaryData::proteanWave_png, BinaryData::proteanWave_pngSize));

    //[UserPreSize]
    //addAndMakeVisible(drawable1.get());
    activity_icon = new IvoxActivityComponent();
    activity_icon->SetBackgroundColour(Colour(0xff283f88));
    viewport.get()->setViewedComponent(activity_icon);
    //[/UserPreSize]

    setSize (600, 400);


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

ControlWindow::~ControlWindow()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    startButton = nullptr;
    helloLabel = nullptr;
    viewport = nullptr;
    drawable1 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void ControlWindow::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff283f88));

    {
        int x = 0, y = 0, width = proportionOfWidth (1.0000f), height = proportionOfHeight (0.2500f);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (Colours::black);
        jassert (drawable1 != 0);
        if (drawable1 != 0)
            drawable1->drawWithin (g, Rectangle<float> (x, y, width, height),
                                   RectanglePlacement::centred, 1.000f);
    }

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void ControlWindow::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    startButton->setBounds (0, proportionOfHeight (0.2500f), 120, 32);
    helloLabel->setBounds (0 + 120, proportionOfHeight (0.2500f), getWidth() - 240, 40);
    viewport->setBounds ((0 + 120) + 0, proportionOfHeight (0.3994f), roundToInt ((getWidth() - 240) * 1.0000f), proportionOfHeight (0.5000f));
    //[UserResized] Add your own custom resize handling here..
    activity_icon->setSize(viewport->getWidth(), viewport->getHeight());
    //[/UserResized]
}

void ControlWindow::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == startButton.get())
    {
        IvoxActivityComponent::Status status = activity_icon->GetStatus();
        if (status < IvoxActivityComponent::PLAY)
            activity_icon->SetStatus((IvoxActivityComponent::Status)(status + 1), true);
        else
            activity_icon->SetStatus(IvoxActivityComponent::DEAD);
    }
    else
    {
        //[UserButtonCode_startButton] -- add your button handler code here ...
        if (buttonThatWasClicked->getToggleState())
        {
            activity_icon->SetStatus(IvoxActivityComponent::IDLE);
        }
        else
        {
            activity_icon->SetStatus(IvoxActivityComponent::DEAD);
        }
        //[/UserButtonCode_startButton]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="ControlWindow" componentName=""
                 parentClasses="public Component" constructorParams="" variableInitialisers=""
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="0" initialWidth="600" initialHeight="400">
  <BACKGROUND backgroundColour="ff283f88">
    <IMAGE pos="0 0 100% 25%" resource="BinaryData::proteanWave_png" opacity="1.00000000000000000000"
           mode="1"/>
  </BACKGROUND>
  <TOGGLEBUTTON name="startButton" id="ff7ec14cd3f75e5e" memberName="startButton"
                virtualName="" explicitFocusOrder="0" pos="0 25% 120 32" tooltip="start stop audio loop"
                txtcol="ffffffff" buttonText="Do Something" connectedEdges="0"
                needsCallback="1" radioGroupId="0" state="0"/>
  <LABEL name="helloLabel" id="cdff1eb1ab965714" memberName="helloLabel"
         virtualName="" explicitFocusOrder="0" pos="0R 25% 240M 40" posRelativeX="ff7ec14cd3f75e5e"
         bkgCol="ffffffff" textCol="ff1e2d75" edTextCol="ff000000" edBkgCol="0"
         labelText="Hello, ProtoWorld" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Default font" fontsize="18.00000000000000000000"
         kerning="0.00000000000000000000" bold="0" italic="0" justification="36"/>
  <VIEWPORT name="viewport" id="e9ab86ac3963528" memberName="viewport" virtualName=""
            explicitFocusOrder="0" pos="0 39.943% 100% 50%" posRelativeX="cdff1eb1ab965714"
            posRelativeW="cdff1eb1ab965714" vscroll="1" hscroll="1" scrollbarThickness="8"
            contentType="0" jucerFile="" contentClass="" constructorParams=""/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]
