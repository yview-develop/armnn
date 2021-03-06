//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ConvertFp16ToFp32Layer.hpp"
#include "LayerCloneBase.hpp"

#include <armnn/TypesUtils.hpp>

#include <backendsCommon/WorkloadData.hpp>
#include <backendsCommon/WorkloadFactory.hpp>

namespace armnn
{

ConvertFp16ToFp32Layer::ConvertFp16ToFp32Layer(const char* name)
    : Layer(1, 1, LayerType::ConvertFp16ToFp32, name)
{
}

std::unique_ptr<IWorkload> ConvertFp16ToFp32Layer::CreateWorkload(const Graph& graph,
    const IWorkloadFactory& factory) const
{
    ConvertFp16ToFp32QueueDescriptor descriptor;
    return factory.CreateConvertFp16ToFp32(descriptor, PrepInfoAndDesc(descriptor, graph));
}

ConvertFp16ToFp32Layer* ConvertFp16ToFp32Layer::Clone(Graph& graph) const
{
    return CloneBase<ConvertFp16ToFp32Layer>(graph, GetName());
}

void ConvertFp16ToFp32Layer::ValidateTensorShapesFromInputs()
{
    VerifyLayerConnections(1, CHECK_LOCATION());

    auto inferredShapes = InferOutputShapes({ GetInputSlot(0).GetConnection()->GetTensorInfo().GetShape() });

    BOOST_ASSERT(inferredShapes.size() == 1);

    ConditionalThrowIfNotEqual<LayerValidationException>(
        "ConvertFp16ToFp32Layer: TensorShape set on OutputSlot[0] does not match the inferred shape.",
        GetOutputSlot(0).GetTensorInfo().GetShape(),
        inferredShapes[0]);
}

void ConvertFp16ToFp32Layer::Accept(ILayerVisitor& visitor) const
{
    // these conversion layers are only inserted by the
    // optimizer and so will never be in an input graph.
    throw armnn::Exception("ConvertFp16ToFp32Layer should never appear in an input graph");
}

} // namespace armnn
