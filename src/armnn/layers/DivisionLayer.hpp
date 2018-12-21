//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "ElementwiseBaseLayer.hpp"

namespace armnn
{

/// This layer represents a division operation.
class DivisionLayer : public ElementwiseBaseLayer
{
public:
    /// Makes a workload for the Division type.
    /// @param [in] graph The graph where this layer can be found.
    /// @param [in] factory The workload factory which will create the workload.
    /// @return A pointer to the created workload, or nullptr if not created.
    virtual std::unique_ptr<IWorkload> CreateWorkload(const Graph& graph,
                                                      const IWorkloadFactory& factory) const override;

    /// Creates a dynamically-allocated copy of this layer.
    /// @param [in] graph The graph into which this layer is being cloned.
    DivisionLayer* Clone(Graph& graph) const override;

protected:
    /// Constructor to create a DivisionLayer.
    /// @param [in] name Optional name for the layer.
    DivisionLayer(const char* name);

    /// Default destructor
    ~DivisionLayer() = default;
};

} // namespace
