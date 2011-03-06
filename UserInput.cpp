#include "UserInput.h"
#include "RenderContext.h"
#include "WorldModel.h"

UserInput::UserInput(unsigned playerID_, unsigned timestamp_) : inputs(0)
                                                              , timestamp(timestamp_)
                                                              , playerID(playerID_)
                                                              , keyDown(false)
                                                              , keyReleased(false)
{
}


void
UserInput::LoadInput(RenderContext& context)
{
    static int sLastMouseX = 0;
    static int sLastMouseY = 0;
    static bool sMouseInitialized = false;

    sf::Event evt;
    while (context.GetWindow()->GetEvent(evt)) {

        /*
         * Local inputs. These get applied directly to the rendering context
         * and aren't communicated over the network.
         */
        switch (evt.Type) {
            case sf::Event::Closed:
                context.GetWindow()->Close();
                break;
            case sf::Event::Resized:
                context.SetViewportAndProjection();
                break;
            case sf::Event::KeyPressed:
                switch(evt.Key.Code) {
                    case sf::Key::W:
                        context.MoveCamera(1.0, 0.0);
                        break;
                    case sf::Key::S:
                        context.MoveCamera(-1.0, 0.0);
                        break;
                    case sf::Key::A:
                        context.MoveCamera(0.0, -1.0);
                        break;
                    case sf::Key::D:
                        context.MoveCamera(0.0, 1.0);
                        break;
                    case sf::Key::Left:
                        context.MoveLight(0.0, -0.1);
                        break;
                    case sf::Key::Right:
                        context.MoveLight(0.0, 0.1);
                        break;
                    case sf::Key::Down:
                        context.MoveLight(-0.1, 0.0);
                        break;
                    case sf::Key::Up:
                        context.MoveLight(0.1, 0.0);
                        break;

                    default:
                        break;
                }
                break;
            case sf::Event::MouseMoved:

                // If we're not initialized, calibrate
                if (!sMouseInitialized) {
                    sLastMouseX = evt.MouseMove.X;
                    sLastMouseY = evt.MouseMove.Y;
                    sMouseInitialized = true;
                }

                // Pan the camera
                // The arguments are +pitch and +yaw. Positive pitch looks up,
                // positive yaw looks right.
                // Y is relative to the top of the window, X is relative to the
                // left of the window.
                context.PanCamera(-(evt.MouseMove.Y - sLastMouseY),
                                  evt.MouseMove.X - sLastMouseX);

                // Save the new current value for next time
                sLastMouseX = evt.MouseMove.X;
                sLastMouseY = evt.MouseMove.Y;
                break;

            default:
                break;
        }

        /*
         * Global inputs. These affect the state of other players. As such,
         * we record them, and then apply them later.
         */
        switch (evt.Type) {
            case sf::Event::KeyPressed:
                switch(evt.Key.Code) {
                    case sf::Key::I:
                        inputs |= USERINPUT_MASK_GROW;
                        break;
                    case sf::Key::K:
                        inputs |= USERINPUT_MASK_SHRINK;
                        break;
                    case sf::Key::T:
                        inputs |= USERINPUT_MASK_UP;
                        break;
                    case sf::Key::G:
                        inputs |= USERINPUT_MASK_DOWN;
                        break;
                    case sf::Key::F:
                        inputs |= USERINPUT_MASK_LEFT;
                        break;
                    case sf::Key::H:
                        inputs |= USERINPUT_MASK_RIGHT;
                        break;
                    default:
                        break;
                }
                break;
            case sf::Event::KeyReleased:
                keyReleased = true;
                break;
            default:
                break;
        }
    }
}

void
UserInput::ApplyInput(WorldModel& model)
{
    if(inputs && keyDown == false) {
        keyDown = true;
    }
    if(keyReleased) {
        keyDown = false;
        inputs = 0;
    }
    if(keyDown && inputs & USERINPUT_MASK_GROW)
        model.GrowPlayer(playerID);
    if (keyDown && inputs & USERINPUT_MASK_SHRINK)
        model.ShrinkPlayer(playerID);
    if (keyDown && inputs & USERINPUT_MASK_UP)
        model.MovePlayer(playerID, USERINPUT_MASK_UP);
    if (keyDown && inputs & USERINPUT_MASK_DOWN)
        model.MovePlayer(playerID, USERINPUT_MASK_DOWN);
    if (keyDown && inputs & USERINPUT_MASK_LEFT)
        model.MovePlayer(playerID, USERINPUT_MASK_LEFT);
    if (keyDown && inputs & USERINPUT_MASK_RIGHT)
        model.MovePlayer(playerID, USERINPUT_MASK_RIGHT);
}

void
UserInput::resetInputState()
{
    keyReleased = false;
}
