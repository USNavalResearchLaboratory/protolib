#ifndef _PROTO_COVER_SET
#define _PROTO_COVER_SET

#include "protoBitmask.h"
#include "protoQueue.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <vector>


// TBD - move implementation of methods to .cpp file

class ProtoCoverSet
{
    public:

        // Solver algorithm types
        enum Algorithm
        {
            GREEDY_MIN_ROWS,
            GREEDY_MIN_COST,
            EXACT_MIN_ROWS,
            EXACT_MIN_COST
        };

        // Column:
        // The context pointer is caller-owned.  ProtoCoverSet does
        // not delete or otherwise manage the referenced object.
        class Column
        {
            public:
                Column() : context(NULL) {}
                ~Column() {}
                void SetContext(void* theContext)
                    { context = theContext;}
                 void* GetContext() const
                {return context;}
            private:
                Column(const Column&) = delete;
                Column& operator=(const Column&) = delete;
                void* context;
        };  // end class ProtoSet::Column

        // A Row owns:
        //   1. The matrix entry values for this row
        //   2. A coverage bitmask indicating non-zero entries
        //   3. An optional caller-owned context pointer
        class Row
        {
            public:
                Row() : values(NULL), column_count(0), context(NULL) {}
                ~Row() {Destroy();}

                bool Create(unsigned int numColumns)
                {
                    Destroy();
                    column_count = numColumns;
                    if (0 == column_count)
                        return true;
                    if (!coverage_mask.Init(column_count))
                    {
                        column_count = 0;
                        return false;
                    }
                    values = new(std::nothrow) double[column_count];
                    if (NULL == values)
                    {
                        coverage_mask.Destroy();
                        column_count = 0;
                        return false;
                    }
                    for (unsigned int i = 0; i < column_count; ++i)
                    {
                        values[i] = 0.0;
                    }
                    return true;
                }
                void Destroy()
                {
                    if (NULL != values)
                    {
                        delete[] values;
                        values = NULL;
                    }
                    coverage_mask.Destroy();
                    column_count = 0;
                    context      = NULL;
                }

                unsigned int GetColumnCount() const
                    {return column_count;}

                bool SetValue(unsigned int column, double value)
                {
                    if (column >= column_count)
                        return false;
                    values[column] = value;
                    // A non-zero matrix entry means this row covers
                    // the corresponding column.
                    if (0.0 != value)
                        coverage_mask.Set(column);
                    else
                        coverage_mask.Unset(column);
                    return true;
                }

                double GetValue(unsigned int column) const
                {
                    ASSERT(column < column_count);
                    return values[column];
                }

                const ProtoBitmask& GetCoverageMask() const
                    {return coverage_mask;}

                void SetContext(void* theContext)
                    {context = theContext;}

                void* GetContext() const
                    {return context;}

            private:
                Row(const Row&) = delete;
                Row& operator=(const Row&) = delete;

                ProtoBitmask    coverage_mask;
                double*         values;
                unsigned int    column_count;
                void*           context;
        };  // end class ProtoCoverSet::Row
        // Matrix:
        // Matrix owns its Row and Column objects, but does NOT own
        // objects referenced by Row/Column context pointers.
        class Matrix
        {
            public:
                Matrix()
                  : row_array(NULL),
                    column_array(NULL),
                    row_count(0),
                    column_count(0),
                    initialized(false)
                {
                }

                ~Matrix()
                {
                    Destroy();
                }

                bool Create(unsigned int numRows,
                            unsigned int numColumns)
                {
                    Destroy();
                    row_count    = numRows;
                    column_count = numColumns;
                    // Allocate rows.
                    if (0 != row_count)
                    {
                        row_array = new(std::nothrow) Row[row_count];
                        if (NULL == row_array)
                        {
                            Destroy();
                            return false;
                        }
                        for (unsigned int i = 0; i < row_count; ++i)
                        {
                            if (!row_array[i].Create(column_count))
                            {
                                Destroy();
                                return false;
                            }
                        }
                    }
                    // Allocate columns.
                    if (0 != column_count)
                    {
                        column_array = new(std::nothrow) Column[column_count];
                        if (NULL == column_array)
                        {
                            Destroy();
                            return false;
                        }
                    }
                    initialized = true;
                    return true;
                }  // end ProtoCoverSet::Matrix::Create()

                void Destroy()
                {
                    if (NULL != row_array)
                    {
                        delete[] row_array;
                        row_array = NULL;
                    }
                    if (NULL != column_array)
                    {
                        delete[] column_array;
                        column_array = NULL;
                    }
                    row_count    = 0;
                    column_count = 0;
                    initialized = false;
                }

                bool IsInitialized() const
                    {return initialized;}

                unsigned int GetRowCount() const
                    {return row_count;}

                unsigned int GetColumnCount() const
                    {return column_count;}

                // Direct Row / Column access
                Row& GetRow(unsigned int row)
                {
                    ASSERT(row < row_count);
                    return row_array[row];
                }
                const Row& GetRow(unsigned int row) const
                {
                    ASSERT(row < row_count);
                    return row_array[row];
                }
                Column& GetColumn(unsigned int column)
                {
                    ASSERT(column < column_count);

                    return column_array[column];
                }
                const Column& GetColumn(unsigned int column) const
                {
                    ASSERT(column < column_count);

                    return column_array[column];
                }
                // Matrix value access
                bool SetValue(unsigned int row,
                              unsigned int column,
                              double       value)
                {
                    if ((row >= row_count) ||
                        (column >= column_count))
                    {
                        return false;
                    }
                    return row_array[row].SetValue(column, value);
                }  // end ProtoCoverSet::Matrix::SetValue()


                double GetValue(unsigned int row,
                                unsigned int column) const
                {
                    ASSERT(row < row_count);
                    ASSERT(column < column_count);
                    return row_array[row].GetValue(column);
                }  // end ProtoCoverSet::Matrix::GetValue()
                const ProtoBitmask& GetRowCoverage(unsigned int row) const
                {
                    ASSERT(row < row_count);
                    return row_array[row].GetCoverageMask();
                }
                // Convenience Row context access
                bool SetRowContext(unsigned int row, void* context)
                {
                    if (row >= row_count)
                        return false;
                    row_array[row].SetContext(context);
                    return true;
                }
                void* GetRowContext(unsigned int row) const
                {
                    ASSERT(row < row_count);
                    return row_array[row].GetContext();
                }

                // Convenience Column context access
                bool SetColumnContext(unsigned int column, void* context)
                {
                    if (column >= column_count)
                        return false;
                    column_array[column].SetContext(context);
                    return true;
                }
                void* GetColumnContext(unsigned int column) const
                {
                    ASSERT(column < column_count);
                    return column_array[column].GetContext();
                }

            private:
                Matrix(const Matrix&) = delete;
                Matrix& operator=(const Matrix&) = delete;

                Row*            row_array;
                Column*         column_array;
                unsigned int    row_count;
                unsigned int    column_count;
                bool            initialized;
        };  // end class ProtoCoverSet::Matrix


        // Solver result
        struct Result
        {
            Result() : feasible(false), objective(0.0) {}

            void Clear()
            {
                feasible  = false;
                objective = 0.0;
                rows.clear();
                row_costs.clear();
            }
            bool feasible;
            // For MIN_ROWS:
            //      Number of selected rows.
            // For MIN_COST:
            //      Total accumulated cost.
            double objective;
            // Selected row indices, in selection order.
            std::vector<unsigned int> rows;
            // Cost incurred by each corresponding selection.
            // For MIN_ROWS each value is 1.0.
            std::vector<double> row_costs;
        };  // end struct ProtoCoverSet::Result


        // Dynamic row-cost callback
        // "row" is the candidate Matrix row.
        // "useful" contains only columns newly covered by the row
        // in the current coverage state.
        //
        // The callback can access:
        //     matrix.GetRow(row).GetContext()
        //     matrix.GetColumn(column).GetContext()
        //     matrix.GetValue(row, column)
        //
        // "userData" is optional additional solve-specific context.
        typedef double (*CostFunction)
        (
            const Matrix&       matrix,
            unsigned int        row,
            const ProtoBitmask& useful,
            void*               userData
        );

        // Solver
        class Solver
        {
            public:
                explicit Solver(const Matrix& theMatrix) : matrix(theMatrix) {}
                // Main solver interface
                // For *_MIN_COST, MaxEntryCost() is used by default.
                bool Solve(Algorithm    algorithm,
                           Result&      result,
                           CostFunction costFunction = NULL,
                           void*        userData = NULL) const
                {
                    result.Clear();
                    if (!matrix.IsInitialized())
                        return false;
                    // No columns means there is nothing to cover.
                    if (0 == matrix.GetColumnCount())
                    {
                        result.feasible = true;
                        return true;
                    }
                    switch (algorithm)
                    {
                        case GREEDY_MIN_ROWS:
                            return SolveGreedy(false, NULL, NULL, result);
                        case GREEDY_MIN_COST:
                            if (NULL == costFunction)
                                costFunction = MaxEntryCost;
                            return SolveGreedy(true, costFunction, userData, result);
                        case EXACT_MIN_ROWS:
                            return SolveExact(false, NULL, NULL, result);
                        case EXACT_MIN_COST:
                            if (NULL == costFunction)
                                costFunction = MaxEntryCost;
                            return SolveExact(true, costFunction, userData, result);
                        default:
                            return false;
                    }
                }  // end ProtoCoverSet::Solver::Solve()

                // Default dynamic cost function
                // Row cost =
                //     maximum matrix value among columns newly covered
                //     by this row.
                static double MaxEntryCost(
                    const Matrix&       matrix,
                    unsigned int        row,
                    const ProtoBitmask& useful,
                    void*               userData)
                {
                    (void)userData;
                    const Row& theRow = matrix.GetRow(row);
                    unsigned int column;
                    if (!useful.GetFirstSet(column))
                        return 0.0;
                    double cost = -std::numeric_limits<double>::infinity();
                    do
                    {
                        cost = std::max(cost, theRow.GetValue(column));
                        ++column;
                    } while (useful.GetNextSet(column));
                    return cost;
                }

            private:
                // Exact-solver dynamic-programming state
                // Each State belongs to:
                //     1. StateIndex
                //     2. Exactly one state_list[k]
                // StateIndex owns the State allocation.
                // "covered" becomes immutable after insertion into
                // StateIndex because it is the radix-tree key.
                class State : public ProtoQueue::Item
                {
                    public:
                        State() : cost(0.0), step_cost(0.0), prev(NULL), row(~0u) {}
                        ~State()
                        {
                            // Required for ProtoQueue::Item subclasses so
                            // residual queue memberships are safely removed.
                            Cleanup();
                        }
                        bool Init(unsigned int numColumns)
                            {return covered.Init(numColumns);}

                        ProtoBitmask covered;

                        // Best known accumulated cost to reach this state.
                        double cost;

                        // Cost of transition from "prev" to this State.
                        double step_cost;

                        // Best predecessor State.
                        State* prev;

                        // Row selected for prev -> this.
                        unsigned int row;
                };  // end class ProtoCoverSet::State

                // Sparse radix-tree DP index
                // Key = State::covered
                class StateIndex : public ProtoIndexedQueueTemplate<State>
                {
                    public:
                        StateIndex(bool usePool = false)
                          : ProtoIndexedQueueTemplate<State>(usePool) {}

                        const char* GetKey(const ProtoQueue::Item& item) const
                        {
                            const State& state =
                                static_cast<const State&>(item);
                            return reinterpret_cast<const char*>(
                                state.covered.mask);
                        }

                        unsigned int GetKeysize(const ProtoQueue::Item& item) const
                        {
                            const State& state =
                                static_cast<const State&>(item);
                            // ProtoIndexedQueue key size is expressed in bits.
                            return state.covered.num_bits;
                        }

                        State* FindState( const ProtoBitmask& covered) const
                        {
                            return Find(reinterpret_cast<const char*>( covered.mask), covered.num_bits);
                        }
                };  // end class ProtoCoverSet::StateIndex

                class StateList : public ProtoSimpleQueueTemplate<State> {};

                // Count set bits in a ProtoBitmask
                static unsigned int CountBits(const ProtoBitmask& bitmask)
                {
                    unsigned int count = 0;
                    for (unsigned int i = 0; i < bitmask.mask_len; ++i)
                    {
                        count += ProtoBitmask::GetWeight(bitmask.mask[i]);
                    }
                    return count;
                }

                // Allocate a new State.
                // Ownership transfers to StateIndex once the State has
                // been successfully inserted there.

                static State* CreateState( unsigned int numColumns)
                {
                    State* state =
                        new(std::nothrow) State;
                    if (NULL == state)
                        return NULL;
                    if (!state->Init(numColumns))
                    {
                        delete state;
                        return NULL;
                    }
                    return state;
                }

                // Exact solver cleanup
                // First remove the State objects from their linked-list
                // buckets without deleting them.
                // Then StateIndex::Destroy() deletes every State.
                static void CleanupStates(StateIndex&  stateIndex,
                                          StateList*   stateLists,
                                          unsigned int numColumns)
                {
                    for (unsigned int i = 0; i <= numColumns; ++i)
                    {
                        stateLists[i].Empty();
                    }
                    stateIndex.Destroy();
                }

                // Greedy solver
                bool SolveGreedy(bool         minimizeCost,
                                 CostFunction costFunction,
                                 void*        userData,
                                 Result&      result) const
                {
                    const unsigned int numRows = matrix.GetRowCount();
                    const unsigned int numColumns = matrix.GetColumnCount();
                    ProtoBitmask covered;
                    ProtoBitmask useful;
                    if (!covered.Init(numColumns) || !useful.Init(numColumns))
                    {
                        return false;
                    }
                    covered.Clear();

                    unsigned int coveredCount = 0;
                    while (coveredCount < numColumns)
                    {
                        unsigned int bestRow = ~0u;
                        unsigned int bestGain = 0;
                        double bestMetric = std::numeric_limits<double>::infinity();
                        double bestStepCost = 0.0;
                        // Evaluate every row against current coverage.
                        for (unsigned int row = 0; row < numRows; ++row)
                        {
                            // useful =
                            //     rowCoverage & ~covered
                            // XCopy() performs:
                            //     this = b & ~this
                            useful.Copy(covered);
                            useful.XCopy(matrix.GetRowCoverage(row));
                            if (!useful.IsSet())
                                continue;
                            const unsigned int gain = CountBits(useful);
                            // Greedy minimum number of rows
                            if (!minimizeCost)
                            {
                                if (gain > bestGain)
                                {
                                    bestGain = gain;
                                    bestRow  = row;
                                }
                            }
                            // Greedy dynamic cost
                            //             current row cost
                            // metric = -----------------------
                            //           newly covered columns
                            else
                            {
                                const double stepCost = costFunction(matrix,
                                                                     row,
                                                                     useful,
                                                                     userData);
                                if (!std::isfinite(stepCost))
                                    continue;
                                const double metric = stepCost / static_cast<double>(gain);
                                // Prefer greater coverage when metrics tie.
                                if ((metric < bestMetric) ||
                                    ((metric == bestMetric) &&
                                     (gain > bestGain)))
                                {
                                    bestMetric   = metric;
                                    bestGain     = gain;
                                    bestRow      = row;
                                    bestStepCost = stepCost;
                                }
                            }
                        }
                        // No row covers any remaining column.
                        if (~0u == bestRow)
                        {
                            result.feasible = false;
                            return true;
                        }
                        if (!minimizeCost) bestStepCost = 1.0;
                        result.rows.push_back(bestRow);
                        result.row_costs.push_back(bestStepCost);
                        result.objective += bestStepCost;
                        // Update coverage.
                        // bestGain is exactly the number of newly
                        // covered columns.
                        covered.Add( matrix.GetRowCoverage(bestRow));
                        coveredCount += bestGain;
                    }
                    result.feasible = true;
                    return true;
                }  // end ProtoCoverSet::Solver::SolveGreedy()

                // Exact sparse dynamic-programming solver
                bool SolveExact(bool         minimizeCost,
                                CostFunction costFunction,
                                void*        userData,
                                Result&      result) const
                {
                    const unsigned int numRows = matrix.GetRowCount();
                    const unsigned int numColumns = matrix.GetColumnCount();
                    // Sparse lookup of all reachable coverage states.
                    // StateIndex also owns/deletes all State objects.
                    StateIndex stateIndex;
                    // stateLists[k] contains States having exactly
                    // k covered columns.
                    // These queues do not own the States.
                    StateList* stateLists = new(std::nothrow) StateList[numColumns + 1];
                    if (NULL == stateLists)
                        return false;
                    // Initial State:
                    //     covered = {}
                    //     cost    = 0
                    State* initial = CreateState(numColumns);
                    if (NULL == initial)
                    {
                        delete[] stateLists;
                        return false;
                    }
                    initial->covered.Clear();
                    initial->cost      = 0.0;
                    initial->step_cost = 0.0;
                    initial->prev      = NULL;
                    initial->row       = ~0u;
                    // Once inserted into StateIndex, ownership of
                    // "initial" belongs to StateIndex.
                    if (!stateIndex.Insert(*initial))
                    {
                        delete initial;
                        delete[] stateLists;
                        return false;
                    }
                    if (!stateLists[0].Append(*initial))
                    {
                        CleanupStates(stateIndex, stateLists, numColumns);
                        delete[] stateLists;
                        return false;
                    }
                    // Reusable transition masks
                    ProtoBitmask useful;
                    ProtoBitmask next;
                    if (!useful.Init(numColumns) || !next.Init(numColumns))
                    {
                        CleanupStates(stateIndex, stateLists, numColumns);
                        delete[] stateLists;
                        return false;
                    }
                    // Dynamic programming
                    //
                    // States are processed in increasing number of
                    // covered columns.
                    //
                    // Every valid transition covers at least one new
                    // column, so it always moves to a higher-numbered
                    // stateLists[] bucket.
                    for (unsigned int k = 0; k < numColumns; ++k)
                    {
                        StateList::Iterator iterator(stateLists[k]);
                        State* current;
                        while (NULL != (current = iterator.GetNextItem()))
                        {
                            // Try every row as the next selection.
                            for (unsigned int row = 0; row < numRows; ++row)
                            {
                                // useful =
                                //  rowCoverage &
                                //     ~current->covered
                                // ProtoBitmask::XCopy(b):
                                //      this = b & ~this
                                useful.Copy(current->covered);
                                useful.XCopy(matrix.GetRowCoverage(row));
                                // Row adds no new coverage.
                                if (!useful.IsSet())
                                    continue;
                                // next =
                                //     current->covered | useful
                                next.Copy(current->covered);
                                next.Add(useful);
                                // Transition cost
                                double stepCost;
                                if (!minimizeCost)
                                {
                                    // Minimum-row exact solver:
                                    // Every selected row costs one.
                                    stepCost = 1.0;
                                }
                                else
                                {
                                    stepCost = costFunction(matrix,
                                                            row,
                                                            useful,
                                                            userData);
                                }
                                if (!std::isfinite(stepCost))
                                    continue;
                                const double candidate = current->cost + stepCost;
                                if (!std::isfinite(candidate))
                                    continue;
                                // Is this coverage state already known?
                                State* nextState = stateIndex.FindState(next);
                                // First path reaching this state.
                                if (NULL == nextState)
                                {
                                    nextState = CreateState(numColumns);
                                    if (NULL == nextState)
                                    {
                                        CleanupStates(stateIndex, stateLists, numColumns);
                                        delete[] stateLists;
                                        return false;
                                    }
                                    nextState->covered.Copy(next);
                                    nextState->cost = candidate;
                                    nextState->step_cost = stepCost;
                                    nextState->prev = current;
                                    nextState->row = row;
                                    // Once inserted, nextState->covered
                                    // is immutable because it is the
                                    // radix-tree key.
                                    // StateIndex now owns nextState.
                                    if (!stateIndex.Insert(*nextState))
                                    {
                                        delete nextState;
                                        CleanupStates(stateIndex, stateLists, numColumns);
                                        delete[] stateLists;
                                        return false;
                                    }
                                    const unsigned int count = CountBits(nextState->covered);
                                    ASSERT(count > k);
                                    ASSERT(count <= numColumns);
                                    if (!stateLists[count]. Append(*nextState))
                                    {
                                        CleanupStates(stateIndex, stateLists, numColumns);
                                        delete[] stateLists;
                                        return false;
                                    }
                                }
                                // Same coverage state reached through a
                                // cheaper sequence of row selections.
                                else if (candidate < nextState->cost)
                                {
                                    // Do NOT modify:
                                    //     nextState->covered
                                    // and do NOT insert nextState into its
                                    // bucket again.
                                    nextState->cost = candidate;
                                    nextState->step_cost = stepCost;
                                    nextState->prev = current;
                                    nextState->row = row;
                                }
                            }
                        }
                    }
                    // Fully-covered State
                    // There is only one possible bit pattern containing
                    // all numColumns bits.  Therefore stateLists[
                    // numColumns] contains at most one State.
                    State* finalState = stateLists[numColumns].GetHead();
                    if (NULL == finalState)
                    {
                        // No combination of rows covers every column.
                        result.feasible = false;
                        CleanupStates(stateIndex, stateLists, numColumns);
                        delete[] stateLists;
                        return true;
                    }
                    result.feasible = true;
                    result.objective = finalState->cost;
                    // Reconstruct optimal path.
                    // prev pointers walk the selected rows backwards from
                    // the complete-coverage State to the empty State.
                    for (State* state = finalState; NULL != state->prev; state = state->prev)
                    {
                        result.rows.push_back(state->row);
                        result.row_costs.push_back(state->step_cost);
                    }
                    std::reverse(result.rows.begin(),result.rows.end());
                    std::reverse(result.row_costs.begin(), result.row_costs.end());
                    // Cleanup
                    // 1. Empty linked-list buckets without deleting States.
                    // 2. Destroy radix index, which deletes the States.
                    CleanupStates(stateIndex, stateLists, numColumns);
                    delete[] stateLists;
                    return true;
                }  // end ProtoCoverSet::Solver::SolveExact()

            private:
                const Matrix& matrix;

        };  // end class ProtoCoverSet::Solver
};  // end class ProtoCovoeSet

#endif  // _PROTO_COVER_SET
