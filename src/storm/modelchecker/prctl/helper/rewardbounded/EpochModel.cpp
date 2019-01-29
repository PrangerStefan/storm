#include "storm/modelchecker/prctl/helper/rewardbounded/EpochModel.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"

#include "storm/exceptions/UncheckedRequirementException.h"

namespace storm {
    namespace modelchecker {
        namespace helper {
            namespace rewardbounded {



                template<typename ValueType>
                std::vector<ValueType> analyzeTrivialDtmcEpochModel(EpochModel<ValueType, true>& epochModel) {

                    std::vector<ValueType> epochResult;
                    epochResult.reserve(epochModel.epochInStates.getNumberOfSetBits());
                    auto stepSolutionIt = epochModel.stepSolutions.begin();
                    auto stepChoiceIt = epochModel.stepChoices.begin();
                    for (auto const& state : epochModel.epochInStates) {
                        while (*stepChoiceIt < state) {
                            ++stepChoiceIt;
                            ++stepSolutionIt;
                        }
                        if (epochModel.objectiveRewardFilter.front().get(state)) {
                            if (*stepChoiceIt == state) {
                                epochResult.push_back(epochModel.objectiveRewards.front()[state] + *stepSolutionIt);
                            } else {
                                epochResult.push_back(epochModel.objectiveRewards.front()[state]);
                            }
                        } else {
                            if (*stepChoiceIt == state) {
                                epochResult.push_back(*stepSolutionIt);
                            } else {
                                epochResult.push_back(storm::utility::zero<ValueType>());
                            }
                        }
                    }
                    return epochResult;
                }

                template<typename ValueType>
                std::vector<ValueType> analyzeNonTrivialDtmcEpochModel(Environment const& env, EpochModel<ValueType, true>& epochModel, std::vector<ValueType>& x, std::vector<ValueType>& b, std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>>& linEqSolver, boost::optional<ValueType> const& lowerBound, boost::optional<ValueType> const& upperBound) {

                    // Update some data for the case that the Matrix has changed
                    if (epochModel.epochMatrixChanged) {
                        x.assign(epochModel.epochMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
                        storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
                        linEqSolver = linearEquationSolverFactory.create(env, epochModel.epochMatrix);
                        linEqSolver->setCachingEnabled(true);
                        auto req = linEqSolver->getRequirements(env);
                        if (lowerBound) {
                            linEqSolver->setLowerBound(lowerBound.get());
                            req.clearLowerBounds();
                        }
                        if (upperBound) {
                            linEqSolver->setUpperBound(upperBound.get());
                            req.clearUpperBounds();
                        }
                        STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException, "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
                    }

                    // Prepare the right hand side of the equation system
                    b.assign(epochModel.epochMatrix.getRowCount(), storm::utility::zero<ValueType>());
                    std::vector<ValueType> const& objectiveValues = epochModel.objectiveRewards.front();
                    for (auto const& choice : epochModel.objectiveRewardFilter.front()) {
                        b[choice] = objectiveValues[choice];
                    }
                    auto stepSolutionIt = epochModel.stepSolutions.begin();
                    for (auto const& choice : epochModel.stepChoices) {
                        b[choice] += *stepSolutionIt;
                        ++stepSolutionIt;
                    }
                    assert(stepSolutionIt == epochModel.stepSolutions.end());

                    // Solve the minMax equation system
                    linEqSolver->solveEquations(env, x, b);

                    return storm::utility::vector::filterVector(x, epochModel.epochInStates);
                }



                template<typename ValueType>
                std::vector<ValueType> analyzeTrivialMdpEpochModel(OptimizationDirection dir, EpochModel<ValueType, true>& epochModel) {
                    // Assert that the epoch model is indeed trivial
                    assert(epochModel.epochMatrix.getEntryCount() == 0);

                    std::vector<ValueType> epochResult;
                    epochResult.reserve(epochModel.epochInStates.getNumberOfSetBits());

                    auto stepSolutionIt = epochModel.stepSolutions.begin();
                    auto stepChoiceIt = epochModel.stepChoices.begin();
                    for (auto const& state : epochModel.epochInStates) {
                        // Obtain the best choice for this state
                        ValueType bestValue;
                        uint64_t lastChoice = epochModel.epochMatrix.getRowGroupIndices()[state + 1];
                        bool isFirstChoice = true;
                        for (uint64_t choice = epochModel.epochMatrix.getRowGroupIndices()[state]; choice < lastChoice; ++choice) {
                            while (*stepChoiceIt < choice) {
                                ++stepChoiceIt;
                                ++stepSolutionIt;
                            }

                            ValueType choiceValue = storm::utility::zero<ValueType>();
                            if (epochModel.objectiveRewardFilter.front().get(choice)) {
                                choiceValue += epochModel.objectiveRewards.front()[choice];
                            }
                            if (*stepChoiceIt == choice) {
                                choiceValue += *stepSolutionIt;
                            }

                            if (isFirstChoice) {
                                bestValue = std::move(choiceValue);
                                isFirstChoice = false;
                            } else {
                                if (storm::solver::minimize(dir)) {
                                    if (choiceValue < bestValue) {
                                        bestValue = std::move(choiceValue);
                                    }
                                } else {
                                    if (choiceValue > bestValue) {
                                        bestValue = std::move(choiceValue);
                                    }
                                }
                            }
                        }
                        // Insert the solution w.r.t. this choice
                        epochResult.push_back(std::move(bestValue));
                    }
                    return epochResult;
                }

                template<typename ValueType>
                std::vector<ValueType> analyzeNonTrivialMdpEpochModel(Environment const& env, OptimizationDirection dir, EpochModel<ValueType, true>& epochModel, std::vector<ValueType>& x, std::vector<ValueType>& b, std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>>& minMaxSolver, boost::optional<ValueType> const& lowerBound, boost::optional<ValueType> const& upperBound) {

                    // Update some data for the case that the Matrix has changed
                    if (epochModel.epochMatrixChanged) {
                        x.assign(epochModel.epochMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
                        storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> minMaxLinearEquationSolverFactory;
                        minMaxSolver = minMaxLinearEquationSolverFactory.create(env, epochModel.epochMatrix);
                        minMaxSolver->setHasUniqueSolution();
                        minMaxSolver->setOptimizationDirection(dir);
                        minMaxSolver->setCachingEnabled(true);
                        minMaxSolver->setTrackScheduler(true);
                        auto req = minMaxSolver->getRequirements(env, dir, false);
                        if (lowerBound) {
                            minMaxSolver->setLowerBound(lowerBound.get());
                            req.clearLowerBounds();
                        }
                        if (upperBound) {
                            minMaxSolver->setUpperBound(upperBound.get());
                            req.clearUpperBounds();
                        }
                        STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException, "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
                        minMaxSolver->setRequirementsChecked();
                    } else {
                        auto choicesTmp = minMaxSolver->getSchedulerChoices();
                        minMaxSolver->setInitialScheduler(std::move(choicesTmp));
                    }

                    // Prepare the right hand side of the equation system
                    b.assign(epochModel.epochMatrix.getRowCount(), storm::utility::zero<ValueType>());
                    std::vector<ValueType> const& objectiveValues = epochModel.objectiveRewards.front();
                    for (auto const& choice : epochModel.objectiveRewardFilter.front()) {
                        b[choice] = objectiveValues[choice];
                    }
                    auto stepSolutionIt = epochModel.stepSolutions.begin();
                    for (auto const& choice : epochModel.stepChoices) {
                        b[choice] += *stepSolutionIt;
                        ++stepSolutionIt;
                    }
                    assert(stepSolutionIt == epochModel.stepSolutions.end());

                    // Solve the minMax equation system
                    minMaxSolver->solveEquations(env, x, b);

                    return storm::utility::vector::filterVector(x, epochModel.epochInStates);
                }

                template<>
                std::vector<double> EpochModel<double, true>::analyzeSingleObjective(
                        const storm::Environment &env, std::vector<double> &x, std::vector<double> &b,
                        std::unique_ptr<storm::solver::LinearEquationSolver<double>> &linEqSolver,
                        const boost::optional<double> &lowerBound, const boost::optional<double> &upperBound) {
                    STORM_LOG_ASSERT(epochMatrix.hasTrivialRowGrouping(), "This operation is only allowed if no nondeterminism is present.");
                    STORM_LOG_ASSERT(equationSolverProblemFormat.is_initialized(), "Unknown equation problem format.");
                    // If the epoch matrix is empty we do not need to solve a linear equation system
                    bool convertToEquationSystem = (equationSolverProblemFormat == storm::solver::LinearEquationSolverProblemFormat::EquationSystem);
                    if ((convertToEquationSystem && epochMatrix.isIdentityMatrix()) || (!convertToEquationSystem && epochMatrix.getEntryCount() == 0)) {
                        return analyzeTrivialDtmcEpochModel<double>(*this);
                    } else {
                        return analyzeNonTrivialDtmcEpochModel<double>(env, *this, x, b, linEqSolver, lowerBound, upperBound);
                    }
                }

                template<>
                std::vector<double> EpochModel<double, true>::analyzeSingleObjective(
                        const storm::Environment &env, storm::OptimizationDirection dir, std::vector<double> &x,
                        std::vector<double> &b,
                        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<double>> &minMaxSolver,
                        const boost::optional<double> &lowerBound, const boost::optional<double> &upperBound) {
                    // If the epoch matrix is empty we do not need to solve a linear equation system
                    if (epochMatrix.getEntryCount() == 0) {
                        return analyzeTrivialMdpEpochModel<double>(dir, *this);
                    } else {
                        return analyzeNonTrivialMdpEpochModel<double>(env, dir, *this, x, b, minMaxSolver, lowerBound, upperBound);
                    }
                }

                template<>
                std::vector<storm::RationalNumber> EpochModel<storm::RationalNumber, true>::analyzeSingleObjective(
                        const storm::Environment &env, std::vector<storm::RationalNumber> &x, std::vector<storm::RationalNumber> &b,
                        std::unique_ptr<storm::solver::LinearEquationSolver<storm::RationalNumber>> &linEqSolver,
                        const boost::optional<storm::RationalNumber> &lowerBound, const boost::optional<storm::RationalNumber> &upperBound) {
                    STORM_LOG_ASSERT(epochMatrix.hasTrivialRowGrouping(), "This operation is only allowed if no nondeterminism is present.");
                    STORM_LOG_ASSERT(equationSolverProblemFormat.is_initialized(), "Unknown equation problem format.");
                    // If the epoch matrix is empty we do not need to solve a linear equation system
                    bool convertToEquationSystem = (equationSolverProblemFormat == storm::solver::LinearEquationSolverProblemFormat::EquationSystem);
                    if ((convertToEquationSystem && epochMatrix.isIdentityMatrix()) || (!convertToEquationSystem && epochMatrix.getEntryCount() == 0)) {
                        return analyzeTrivialDtmcEpochModel<storm::RationalNumber>(*this);
                    } else {
                        return analyzeNonTrivialDtmcEpochModel<storm::RationalNumber>(env, *this, x, b, linEqSolver, lowerBound, upperBound);
                    }
                }

                template<>
                std::vector<storm::RationalNumber> EpochModel<storm::RationalNumber, true>::analyzeSingleObjective(
                        const storm::Environment &env, storm::OptimizationDirection dir, std::vector<storm::RationalNumber> &x,
                        std::vector<storm::RationalNumber> &b,
                        std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<storm::RationalNumber>> &minMaxSolver,
                        const boost::optional<storm::RationalNumber> &lowerBound, const boost::optional<storm::RationalNumber> &upperBound) {
                    // If the epoch matrix is empty we do not need to solve a linear equation system
                    if (epochMatrix.getEntryCount() == 0) {
                        return analyzeTrivialMdpEpochModel<storm::RationalNumber>(dir, *this);
                    } else {
                        return analyzeNonTrivialMdpEpochModel<storm::RationalNumber>(env, dir, *this, x, b, minMaxSolver, lowerBound, upperBound);
                    }
                }

                template class EpochModel<double, true>;
                template class EpochModel<double, false>;
                template class EpochModel<storm::RationalNumber, true>;
                template class EpochModel<storm::RationalNumber, false>;
            }
        }
    }
}
