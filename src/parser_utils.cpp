#include "parser_utils.hpp"
#include "string_utils.hpp"
#include <cstddef>

namespace pu {
    using su::trim; using su::stripTrailingSemicolon; using su::findNoCase; using su::cleanLiteral;

    std::string extractTableNameAfter(const std::string &cmd, const std::string &keyword) {
        std::size_t pos = keyword.empty() ? 0 : su::findNoCase(cmd, keyword);
        if (!keyword.empty() && pos == std::string::npos) return "";
        std::string rest = su::trim(cmd.substr(pos + (keyword.empty()?0:keyword.size())));
        std::size_t end = rest.find_first_of(" \t\n\r(),;");
        if (end == std::string::npos) return su::stripTrailingSemicolon(rest);
        return su::stripTrailingSemicolon(rest.substr(0, end));
    }

    std::vector<std::string> parseParenList(const std::string &s) {
        std::vector<std::string> out; std::string work = su::trim(s);
        if (!work.empty() && work.front()=='(' && work.back()==')') work = work.substr(1, work.size()-2);
        std::string token; bool inS=false,inD=false;
        for (char c: work) {
            if (c=='"' && !inS) { inD=!inD; token+=c; }
            else if (c=='\'' && !inD) { inS=!inS; token+=c; }
            else if (c==',' && !inS && !inD) { out.push_back(su::cleanLiteral(token)); token.clear(); }
            else token+=c;
        }
        if (!token.empty() || work.empty()) out.push_back(su::cleanLiteral(token));
        return out;
    }

    std::vector<std::string> splitCSVOutsideQuotes(const std::string &s) {
        std::vector<std::string> out; std::string token; bool inS=false,inD=false;
        for (char c: s) {
            if (c=='"' && !inS) { inD=!inD; token+=c; }
            else if (c=='\'' && !inD) { inS=!inS; token+=c; }
            else if (c==',' && !inS && !inD) { out.push_back(su::trim(token)); token.clear(); }
            else token+=c;
        }
        if (!token.empty()) out.push_back(su::trim(token));
        return out;
    }

    std::pair<std::string,std::string> parseWhereEquals(const std::string &cmd) {
        std::size_t wherePos = su::findNoCase(cmd, "WHERE");
        if (wherePos == std::string::npos) return {"",""};
        std::string wherePart = su::stripTrailingSemicolon(su::trim(cmd.substr(wherePos + 5)));
        bool inS=false,inD=false; std::size_t eq=std::string::npos;
        for (std::size_t i=0;i<wherePart.size();++i) {
            char c = wherePart[i];
            if (c=='"' && !inS) inD=!inD;
            else if (c=='\'' && !inD) inS=!inS;
            else if (c=='=' && !inS && !inD) { eq=i; break; }
        }
        if (eq==std::string::npos) return {"",""};
        std::string col = su::trim(wherePart.substr(0,eq));
        std::string val = su::cleanLiteral(wherePart.substr(eq+1));
        return {col,val};
    }

    std::unordered_map<std::string,std::string> parseAssignments(const std::string &setPartRaw) {
        std::unordered_map<std::string,std::string> out; std::string setPart = setPartRaw;
        std::size_t setPos = su::findNoCase(setPart, "SET");
        if (setPos != std::string::npos) setPart = setPart.substr(setPos+3);
        setPart = su::stripTrailingSemicolon(su::trim(setPart));
        for (auto &piece : splitCSVOutsideQuotes(setPart)) {
            bool inS=false,inD=false; std::size_t eq=std::string::npos;
            for (std::size_t i=0;i<piece.size();++i) {
                char c = piece[i];
                if (c=='"' && !inS) inD=!inD; else if (c=='\'' && !inD) inS=!inS; else if (c=='=' && !inS && !inD) { eq=i; break; }
            }
            if (eq==std::string::npos) continue;
            std::string key = su::trim(piece.substr(0,eq));
            std::string val = su::cleanLiteral(piece.substr(eq+1));
            if (!key.empty()) out[key]=val;
        }
        return out;
    }
}