#pragma once
#include <algorithm>
#include <cmath>
namespace gate {
// Closed-form critically damped motion is independent of frame rate. A new
// wheel event retargets the same motion instead of restarting an easing curve.
struct ScrollMotion {
    double position=0,target=0,velocity=0;
    void reset(double value) noexcept {position=target=value;velocity=0;}
    void retarget(double value) noexcept {
        target=value;
        if((target-position)*velocity<0)velocity=0;
    }
    bool advance(double seconds) noexcept {
        constexpr double speed=40;
        seconds=std::clamp(seconds,0.,.25);
        const double distance=position-target,c=velocity+speed*distance;
        const double decay=std::exp(-speed*seconds);
        position=target+(distance+c*seconds)*decay;
        velocity=(velocity-speed*c*seconds)*decay;
        if(distance*(position-target)<=0||(std::abs(position-target)<.1&&std::abs(velocity)<1)){reset(target);return false;}
        return true;
    }
};
}
