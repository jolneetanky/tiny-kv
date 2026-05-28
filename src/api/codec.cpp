#include "api/codec.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

namespace protocol
{
    namespace
    {
        bool consumeLiteral(const std::string &line, std::size_t &pos, std::string_view literal)
        {
            if (line.compare(pos, literal.size(), literal) != 0)
            {
                return false;
            }
            pos += literal.size();
            return true;
        }

        bool parseLengthPrefixedField(
            const std::string &line,
            std::size_t &pos,
            std::string_view fieldName,
            std::string &out,
            std::string &err)
        {
            if (!consumeLiteral(line, pos, fieldName))
            {
                err = "expected field " + std::string(fieldName);
                return false;
            }

            const std::size_t lengthStart = pos;
            while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos])))
            {
                ++pos;
            }

            if (lengthStart == pos)
            {
                err = "expected length for field " + std::string(fieldName);
                return false;
            }

            if (pos >= line.size() || line[pos] != ':')
            {
                err = "expected ':' after length for field " + std::string(fieldName);
                return false;
            }

            std::size_t length = 0;
            try
            {
                length = std::stoull(line.substr(lengthStart, pos - lengthStart));
            }
            catch (const std::exception &)
            {
                err = "invalid length for field " + std::string(fieldName);
                return false;
            }

            ++pos; // skip ':'

            if (line.size() - pos < length)
            {
                err = "payload shorter than declared length for field " + std::string(fieldName);
                return false;
            }

            out = line.substr(pos, length);
            pos += length;
            return true;
        }
    } // namespace

    static Command parseCommand(std::string cmd)
    {
        std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                       [](unsigned char c)
                       {
                           // remember a `char` is just an integer
                           // Eg. 'F' = 70 in ASCII.
                           // std::toupper(c) takes in an `unsigned char` (0-255),
                           // and returns an `int`.
                           // explicitly convert the `int` -> `char` for safety.
                           return static_cast<char>(std::toupper(c));
                       });
        if (cmd == "PUT")
            return Command::PUT;
        if (cmd == "GET")
            return Command::GET;
        if (cmd == "DEL")
            return Command::DEL;
        if (cmd == "EXIT")
            return Command::EXIT;
        if (cmd == "_FLUSH")
            return Command::_FLUSH;
        return Command::UNKNOWN;
    }

    std::string encodeRequest(const protocol::Request &req)
    {
        std::string encoded;

        switch (req.command)
        {
        case Command::PUT:
            encoded = "PUT";
            break;
        case Command::GET:
            encoded = "GET";
            break;
        case Command::DEL:
            encoded = "DEL";
            break;
        case Command::EXIT:
            encoded = "EXIT";
            break;
        case Command::_FLUSH:
            encoded = "_FLUSH";
            break;
        default:
            encoded = "UNKNOWN";
            break;
        }

        for (const std::string &arg : req.args)
        {
            encoded.push_back(' ');
            encoded += arg;
        }

        encoded.push_back('\n');
        return encoded;
    }

    // string -> Request
    bool decodeLine(const std::string &line, Request &outReq, std::string &err)
    {
        std::istringstream iss(line); // handles tokenization, eg. "GET mykey" or "GET     mykey" -> ["GET", "mykey"]
        std::string cmdStr;

        // `iss` is a stream (ie. has a buffer and all that)
        // so we stream the first toekn
        // >> reads a token from the stream into `cmdStr`.
        if (!(iss >> cmdStr))
        {
            err = "empty command";
            return false;
        }

        outReq.command = parseCommand(cmdStr);

        outReq.args.clear();

        // read the next tokens from the stream.
        // these tokens are arguments.
        std::string token;
        while (iss >> token)
        {
            outReq.args.push_back(token);
        }

        err.clear();
        return true;
    }

    // IMPT: end with "\n"
    // looks like eg. "status=OK message=2:OK data=5:value\n"
    std::string encodeResponse(const Response &resp)
    {
        const std::string status = resp.ok ? "OK" : "ERR";
        const std::string &data = resp.data.value_or("");

        std::string encoded;
        encoded += "status=";
        encoded += status;
        encoded += " message=";
        encoded += std::to_string(resp.message.size());
        encoded += ":";
        encoded += resp.message;
        encoded += " data=";
        encoded += std::to_string(resp.data.has_value() ? data.size() : 0);
        encoded += ":";
        if (resp.data.has_value())
        {
            encoded += data;
        }
        encoded += "\n";
        return encoded;
    }

    // string -> Response
    bool decodeResponseLine(const std::string &line, protocol::Response &outRes, std::string &err)
    {
        outRes.ok = false;
        outRes.message.clear();
        outRes.data.reset();

        std::size_t pos = 0;
        // check if status field is present
        if (!consumeLiteral(line, pos, "status="))
        {
            err = "expected status field";
            return false;
        }

        const std::size_t statusStart = pos;
        while (pos < line.size() && line[pos] != ' ')
        {
            ++pos;
        }

        const std::string status = line.substr(statusStart, pos - statusStart);
        if (status == "OK")
        {
            outRes.ok = true;
        }
        else if (status == "ERR")
        {
            outRes.ok = false;
        }
        else
        {
            err = "invalid response status";
            return false;
        }

        if (!consumeLiteral(line, pos, " "))
        {
            err = "expected space after status field";
            return false;
        }

        std::string message;
        if (!parseLengthPrefixedField(line, pos, "message=", message, err))
        {
            return false;
        }
        outRes.message = std::move(message);

        if (!consumeLiteral(line, pos, " "))
        {
            err = "expected space after message field";
            return false;
        }

        std::string data;
        if (!parseLengthPrefixedField(line, pos, "data=", data, err))
        {
            return false;
        }
        if (!data.empty())
        {
            outRes.data = std::move(data);
        }

        if (pos < line.size() && line[pos] == '\n')
        {
            ++pos;
        }

        if (pos != line.size())
        {
            err = "unexpected trailing response bytes";
            return false;
        }

        err.clear();
        return true;
    }
} // namespace protocol
