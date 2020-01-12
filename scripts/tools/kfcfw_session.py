#
# Author: sascha_lammers@gmx.de
#

import secrets
import hashlib

class Session:

    def __init__(self, hash = hashlib.sha1(), rounds = 1024):
        self.rounds = rounds
        self.hash = hash

    def generate(self, username, password, salt = None):

        if salt==None:
            salt = secrets.token_bytes(8)

        hash = self.hash.copy()

        hash.update(salt)
        hash.update(password.encode('utf8'))
        hash.update(username.encode('utf8'))
        dig1 = hash.digest()

        for n in range(0, self.rounds):
            hash = self.hash.copy()
            hash.update(dig1)
            hash.update(salt)
            dig1 = hash.digest()

        return salt.hex() + hash.hexdigest()

    def verify(self, username, password, session_id):

        salt = bytearray.fromhex(session_id[0:16])
        return (self.generate(username, password, salt) == session_id)


def test():
    session = Session()
    # session = Session(hashlib.sha256(), 128) # ESP32

    sid = session.generate('username', '12345678')
    print("SID " + sid)
    print("Valid " + str(session.verify('username', '12345678', sid)))
