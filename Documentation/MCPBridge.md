# MCP Bridge

The MCP bridge lets AI tools interact with, inspect, and control a running Hyperion Editor instance.

## 1. Enable the plugin

Open your project, then open the gear menu in the toolbar (right hand side) and choose **Plugins...**.
Turn on **MCPBridge** and click **Apply**.

If everything working correctly, you should see a line in your console log:

```
MCP bridge listening on http://127.0.0.1:2673/mcp
```

This setting is saved with the project. You'll need to enable it for any projects you want to use the MCP Bridge plugin in.

## 2. Connect your AI tool

For Claude Code:

```bash
claude mcp add --transport http hyperion http://127.0.0.1:2673/mcp
```

For other tools, add an HTTP MCP server with the URL `http://127.0.0.1:2673/mcp`.
