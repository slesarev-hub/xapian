/** @file editdistance.h
 * @brief Edit distance calculation algorithm.
 */
/* Copyright (C) 2003 Richard Boulton
 * Copyright (C) 2007,2008,2017,2019 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef XAPIAN_INCLUDED_EDITDISTANCE_H
#define XAPIAN_INCLUDED_EDITDISTANCE_H

#include <cstdlib>
#include <climits>
#include <vector>

#include "omassert.h"
#include "xapian/unicode.h"

/** Calculate edit distances to a target string.
 *
 *  Edit distance is defined as the minimum number of edit operations
 *  required to move from one sequence to another.  The edit operations
 *  considered are:
 *   - Insertion of a character at an arbitrary position.
 *   - Deletion of a character at an arbitrary position.
 *   - Substitution of a character at an arbitrary position.
 *   - Transposition of two neighbouring characters at an arbitrary position
 *     in the string.
 */
class EditDistanceCalculator {
    /// Don't allow assignment.
    EditDistanceCalculator& operator=(const EditDistanceCalculator&) = delete;

    /// Don't allow copying.
    EditDistanceCalculator(const EditDistanceCalculator&) = delete;

    /// Target in UTF-32.
    std::vector<unsigned> target;

    /// Current candidate in UTF-32.
    mutable std::vector<unsigned> utf32;

    mutable int* array = nullptr;

    // We sum the character frequency histogram absolute differences to compute
    // a lower bound on the edit distance.  Rather than counting each Unicode
    // code point uniquely, we use an array with VEC_SIZE elements and tally
    // code points modulo VEC_SIZE which can only reduce the bound we
    // calculate.
    //
    // There will be a trade-off between how good the bound is and how large
    // and array is used (a larger array takes more time to clear and sum
    // over).  The value 64 is somewhat arbitrary - it works as well as 128 for
    // the testsuite but that may not reflect real world performance.
    // FIXME: profile and tune.
    static constexpr int VEC_SIZE = 64;

    /** Frequency histogram for target sequence.
     *
     *  Note: C++ will default initialise all remaining elements.
     */
    int target_freqs[VEC_SIZE] = { 0 };

    /** Calculate edit distance.
     *
     *  Internal helper - the cheap case is inlined from the header.
     */
    int calc(const unsigned* ptr, int len, int max_distance) const;

  public:
    /** Constructor.
     *
     *  @param target_	Target string to calculate edit distances to.
     */
    explicit
    EditDistanceCalculator(const std::string& target_) {
	using Xapian::Utf8Iterator;
	for (Utf8Iterator it(target_); it != Utf8Iterator(); ++it) {
	    unsigned ch = *it;
	    target.push_back(ch);
	    ++target_freqs[ch % VEC_SIZE];
	}
    }

    ~EditDistanceCalculator() {
	delete [] array;
    }

    /** Calculate edit distance for a sequence.
     *
     *  @param candidate	String to calculate edit distance for.
     *  @param max_distance	The greatest edit distance that's interesting
     *				to us.  If the true edit distance is >
     *				max_distance, any value > max_distance may be
     *				returned instead (which allows the edit
     *				distance algorithm to avoid work for poor
     *				matches).  The value passed for subsequent
     *				calls to this method on the same object must be
     *				the same or less.
     *
     *  @return The edit distance between candidate and the target.
     */
    int operator()(const std::string& candidate, int max_distance) const {
	// There's no point considering a word where the difference in length
	// is greater than the smallest number of edits we've found so far.
	//
	// First check based on the encoded UTF-8 length of the candidate.
	// Each Unicode codepoint is 1-4 bytes in UTF-8 and one word in UTF-32,
	// so the number of UTF-32 characters in candidate must be >= int(bytes
	// + 3 / 4) and <= bytes.
	if (target.size() > candidate.size() + max_distance) {
	    // Candidate too short.
	    return INT_MAX;
	}
	if (target.size() + max_distance < candidate.size() * 3 / 4) {
	    // Candidate too long.
	    return INT_MAX;
	}

	// Now convert to UTF-32.
	utf32.assign(Xapian::Utf8Iterator(candidate), Xapian::Utf8Iterator());

	// Check a cheap length-based lower bound based on UTF-32 lengths.
	int lb = std::abs(int(utf32.size()) - int(target.size()));
	if (lb > max_distance) {
	    return lb;
	}

	// Actually calculate the edit distance.
	return calc(&utf32[0], utf32.size(), max_distance);
    }
};

#endif // XAPIAN_INCLUDED_EDITDISTANCE_H
