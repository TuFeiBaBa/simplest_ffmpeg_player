#include <string>
using namespace std;
class Solution {
public:
	string truncateSentence(string s, int k) {
		for (int i = 0; i < s.size(); i++) {
			if (s[i] == ' ') {
				k--;
			}
			if (k == 0) {
				return s.substr(0, i);
			}
		}
		return s;
	}

	char* truncateSentence(char* s, int k) {
		int len = strlen(s);
		int end = len;
		for (int i = 1; i < len; i++) {
			if (s[i] == ' ') {
				k--;
			}
			if (k == 0) {
				end = i;
			}
		}
		s[end] = '\0';
		return s;
	}
};