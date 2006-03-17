#
# MythTV bindings for perl.
#
# Object containing info about a particular MythTV recording.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

package MythTV::Recording {

    sub new {
        my $class = shift;
        my $self  = {
                     'foo' => 'bar',
                    };
        bless($self, $class);

    # Return
        return $self;
    }

}