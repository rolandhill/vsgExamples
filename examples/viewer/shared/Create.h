#pragma once

#include <vsg/core/Data.h>
#include <vsg/nodes/Node.h>

namespace vsg
{
    vsg::ref_ptr<vsg::Node> createTextureQuad(vsg::ref_ptr<vsg::Data> sourceData);
}
