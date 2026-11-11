#pragma once
#include <vector>

namespace NRetail
{
// Iteration order of retail's STLport integer hash tables (initial hint 10).
// The owning map still provides lookup/storage; register each newly inserted key.
class CIntHashOrder
{
	std::vector<std::vector<int> > buckets;
	size_t count;
	mutable std::vector<int> order;
	mutable bool dirty;
public:
	CIntHashOrder() : buckets( 13 ), count( 0 ), dirty( true ) {}
	void Clear()
	{
		// STLport clear retains the bucket count, including after a rebuild.
		for ( size_t i = 0; i < buckets.size(); ++i )
			buckets[i].clear();
		count = 0;
		dirty = true;
	}
	void Insert( int key )
	{
		if ( count + 1 > buckets.size() )
		{
			static const unsigned primes[] = { 13,29,53,97,193,389,769,1543,3079,6151,12289,
				24593,49157,98317,196613,393241,786433,1572869,3145739,6291469,12582917,
				25165843,50331653,100663319,201326611,402653189,805306457,1610612741,3221225473u,4294967291u };
			size_t nextSize = buckets.size();
			for ( size_t i = 0; i < sizeof(primes)/sizeof(primes[0]); ++i )
				if ( primes[i] >= count + 1 )
				{
					nextSize = primes[i];
					break;
				}
			std::vector<std::vector<int> > next( nextSize );
			for ( size_t i = 0; i < buckets.size(); ++i )
				for ( size_t j = 0; j < buckets[i].size(); ++j )
				{
					const int old = buckets[i][j];
					std::vector<int> &dest = next[static_cast<unsigned>( old ) % nextSize];
					dest.insert( dest.begin(), old );
				}
			buckets.swap( next );
		}
		std::vector<int> &dest = buckets[static_cast<unsigned>( key ) % buckets.size()];
		dest.insert( dest.begin(), key );
		++count;
		dirty = true;
	}
	const std::vector<int>& GetKeys() const
	{
		if ( dirty )
		{
			order.clear();
			order.reserve( count );
			for ( size_t i = 0; i < buckets.size(); ++i )
				order.insert( order.end(), buckets[i].begin(), buckets[i].end() );
			dirty = false;
		}
		return order;
	}
};
}
