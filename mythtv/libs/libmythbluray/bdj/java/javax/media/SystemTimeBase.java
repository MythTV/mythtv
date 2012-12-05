package javax.media;

/**
 * This file is from FMJ (fmj-sf.net)
 * 
 * Complete.
 * @author Ken Larson
 *
 */
public final class SystemTimeBase implements TimeBase
{

    private static long start = -1L; 
    public long getNanoseconds()
    {
        // This version only has millisecond accuracy.
        
        if (start < 0)
        {   start = System.currentTimeMillis();
            return 0;
        }
        return (System.currentTimeMillis() - start) * 1000000L;
        //return System.nanoTime(); // TODO: does this need to be relative to a specific point in time?
    }

    public Time getTime()
    {
        return new Time(getNanoseconds());
    }

}