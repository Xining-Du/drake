#include "drake/multibody/fem/dev/fem_elasticity.h"

#include <gtest/gtest.h>

#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/math/autodiff_gradient.h"
#include "drake/multibody/fem/dev/fem_state.h"
#include "drake/multibody/fem/dev/linear_elasticity_model.h"
#include "drake/multibody/fem/dev/linear_simplex_element.h"
#include "drake/multibody/fem/dev/quadrature.h"

namespace drake {
namespace multibody {
namespace fem {
constexpr int kNaturalDim = 3;
constexpr int kSpatialDim = 3;
constexpr int kProblemDim = 3;
constexpr int kQuadratureOrder = 1;
constexpr int kNumQuads = 1;
constexpr int kNumVertices = 4;
constexpr int kDof = kSpatialDim * kNumVertices;
const ElementIndex kDummyElementIndex(0);

class ElasticityElementTest : public ::testing::Test {
 protected:
  using QuadratureType =
      SimplexGaussianQuadrature<AutoDiffXd, kQuadratureOrder, kSpatialDim>;
  using ShapeType = LinearSimplexElement<AutoDiffXd, kNaturalDim>;
  void SetUp() override { SetupElement(); }

  void SetupElement() {
    std::vector<NodeIndex> node_indices = {NodeIndex(0), NodeIndex(1),
                                           NodeIndex(2), NodeIndex(3)};
    linear_elasticity_ =
        std::make_unique<LinearElasticityModel<AutoDiffXd>>(1, 0.25);
    MatrixX<AutoDiffXd> reference_positions = get_reference_positions();
    const AutoDiffXd DummyDensity(1.23);
    fem_elasticity_ = std::make_unique<
        ElasticityElement<AutoDiffXd, ShapeType, QuadratureType>>(
        kDummyElementIndex, node_indices, DummyDensity,
        std::move(linear_elasticity_), reference_positions);
  }

  void SetupState() {
    state_.Resize(kDof);
    state_.set_qdot(VectorX<AutoDiffXd>::Zero(kDof));
    // Set arbitrary node positions and the gradient.
    Eigen::Matrix<double, kDof, 1> x;
    x << 0.18, 0.63, 0.54, 0.13, 0.92, 0.17, 0.03, 0.86, 0.85, 0.25, 0.53, 0.67;
    const Eigen::Matrix<double, kDof, Eigen::Dynamic> gradient =
        MatrixX<double>::Identity(kDof, kDof);
    const VectorX<AutoDiffXd> x_autodiff =
        math::initializeAutoDiffGivenGradientMatrix(x, gradient);
    state_.set_q(x_autodiff);
    // Set up the element cache.
    auto linear_elasticity_cache =
        std::make_unique<LinearElasticityModelCache<AutoDiffXd>>(
            kDummyElementIndex, kNumQuads);
    std::vector<std::unique_ptr<ElementCache<AutoDiffXd>>> cache;
    // TODO(xuchenhan-tri): Add a method to FemElement that creates a compatible
    // ElementCache.
    cache.emplace_back(std::make_unique<ElasticityElementCache<AutoDiffXd>>(
        kDummyElementIndex, kNumQuads, std::move(linear_elasticity_cache)));
    state_.ResetElementCache(std::move(cache));
  }

  // Set an arbitrary reference positions such that the tetrahedron is not
  // inverted.
  Matrix3X<AutoDiffXd> get_reference_positions() const {
    Matrix3X<AutoDiffXd> Q(kSpatialDim, kNumVertices);
    Q << -0.10, 0.90, 0.02, 0.10, 1.33, 0.23, 0.04, 0.01, 0.20, 0.03, 2.31,
        -0.12;
    return Q;
  }

  // Calculates the negative elastic force at state_.
  VectorX<AutoDiffXd> CalcNegativeElasticForce() const {
    VectorX<AutoDiffXd> neg_force(kDof);
    fem_elasticity_->CalcNegativeElasticForce(state_, &neg_force);
    return neg_force;
  }

  std::unique_ptr<ElasticityElement<AutoDiffXd, ShapeType, QuadratureType>>
      fem_elasticity_;
  std::unique_ptr<LinearElasticityModel<AutoDiffXd>> linear_elasticity_;
  FemState<AutoDiffXd> state_;
};

namespace {
TEST_F(ElasticityElementTest, Basic) {
  EXPECT_EQ(fem_elasticity_->num_nodes(), kNumVertices);
  EXPECT_EQ(fem_elasticity_->num_quads(), kNumQuads);
  EXPECT_EQ(fem_elasticity_->solution_dimension(), kProblemDim);
}

TEST_F(ElasticityElementTest, ElasticForceIsNegativeEnergyDerivative) {
  SetupState();
  AutoDiffXd energy = fem_elasticity_->CalcElasticEnergy(state_);
  VectorX<AutoDiffXd> neg_force = CalcNegativeElasticForce();
  EXPECT_TRUE(CompareMatrices(energy.derivatives(), neg_force,
                              std::numeric_limits<double>::epsilon()));
  // TODO(xuchenhan-tri) Modify this to account for damping forces and inertia
  // terms.
  VectorX<AutoDiffXd> residual(kDof);
  fem_elasticity_->CalcResidual(state_, &residual);
  EXPECT_TRUE(CompareMatrices(residual, neg_force));
}
// TODO(xuchenhan-tri): Add TEST_F as needed for damping and inertia terms
// separately.
}  // namespace
}  // namespace fem
}  // namespace multibody
}  // namespace drake
