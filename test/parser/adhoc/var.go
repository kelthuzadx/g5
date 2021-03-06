package main

var tests = []struct {
		val    interface{} // type as a value
		_32bit uintptr     // size on 32bit platforms
		_64bit uintptr     // size on 64bit platforms
	}{
		{runtime.G{}, 216, 376}, // g, but exported for testing
	}

var (
	mbuckets  *bucket // memory profile buckets
	bbuckets  *bucket // blocking profile buckets
	xbuckets  *bucket // mutex profile buckets
	buckhash  *[179999]*bucket
	bucketmem uintptr

	mProf struct {
		// All fields in mProf are protected by proflock.

		// cycle is the global heap profile cycle. This wraps
		// at mProfCycleWrap.
		cycle uint32
		// flushed indicates that future[cycle] in all buckets
		// has been flushed to the active profile.
		flushed bool
	}
)
var blockJump = [...]struct {
	asm, invasm obj.As
}{
	ssa.Block386EQ:  {x86.AJEQ, x86.AJNE},
	ssa.Block386NE:  {x86.AJNE, x86.AJEQ},
}
// HTML entities that are two unicode codepoints.
var entity2 map[string][2]rune

var entity map[string]rune

// populateMapsOnce guards calling populateMaps.
var populateMapsOnce sync.Once

var smallPowersOfTen = [...]extFloat{
	{1 << 63, -63, false},        // 1
	{0xa << 60, -60, false},      // 1e1
}
var zeroProcAttr ProcAttr
var zeroSysProcAttr SysProcAttr

var digestSizes = []uint8{
	MD4:         16,
	MD5:         16,
	SHA1:        20,
}

var replacementTable = [...]rune{
	'\u20AC', // First entry is what 0x80 should be replaced with.
	'\u0081',
	'\u201A',
	'\u0192',
}
