#pragma once
#include <iterator>
#include <vector>

namespace NRetail
{
// Retail's strided merge sort takes equal keys from the second chain first.
// std::sort/stable_sort cannot reproduce its material and terrain-spot ordering.
template<class TIterator, class TLess>
void Sort( TIterator first, TIterator last, TLess less )
{
	const size_t n = last - first;
	if ( n < 2 )
		return;
	std::vector<size_t> order( n ), next( n );
	for ( size_t i = 0; i < n; ++i )
		order[i] = i;
	size_t step = 1;
	while ( step <= ( n - 1 ) / 2 )
		step *= 2;
	for ( ; step; step /= 2 )
	{
		if ( step > n - step )
			for ( size_t i = n - step; i < n; ++i )
				next[i] = order[i];
		for ( size_t j = 0; j < step && j < n - step; ++j )
		{
			size_t a = j, b = j + step, out = j;
			while ( a < n && b < n )
			{
				const bool takeA = less( *( first + order[a] ), *( first + order[b] ) );
				size_t &src = takeA ? a : b;
				next[out] = order[src];
				src += 2 * step;
				out += step;
			}
			for ( ; a < n; a += 2 * step, out += step )
				next[out] = order[a];
			for ( ; b < n; b += 2 * step, out += step )
				next[out] = order[b];
		}
		order.swap( next );
	}
	typedef typename std::iterator_traits<TIterator>::value_type TValue;
	const std::vector<TValue> values( first, last );
	for ( size_t i = 0; i < n; ++i )
		*( first + i ) = values[order[i]];
}
}
