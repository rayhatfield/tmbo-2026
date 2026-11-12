<?php
	// Include, and check we've got a connection to the database.
	require_once("admin/mysqlConnectionInfo.inc");
	if(!isset($link) || !$link) $link = openDbConnection();
	require_once("offensive/assets/functions.inc");
	require_once("offensive/assets/tabs.inc");
	require_once("offensive/assets/classes.inc");

	function title() {
		global $upload;
		return "[discuss] : " . $upload->filename();
	}

	function start() {
		global $upload, $uploader;
		mustLogIn();
		
		if( ! is_intger( $_REQUEST['fileid'] ) ){
			header( "Location: ./" );
			exit;
		}
		
		$upload = core_getupload($_REQUEST['fileid']);
		$uploader = $upload->uploader();

		$upload->clearSubscription();

		// update pickuplink for discussions
		if($upload->type() == "topic") {
			PickupLink::content($upload->type());
			PickupLink::update($upload);
		}

	
	  // check to see if someone's posting.  
	  /* check if someone is being hoodwinked into voting by link.
	   * NB: only works if the client is reporting REFERERS. 
	   */
	  if($upload->type() != 'topic' &&
		 array_key_exists("submit", $_REQUEST) && array_key_exists("HTTP_REFERER", $_SERVER) &&
	     $_SERVER['HTTP_REFERER'] !== "" && strpos($_SERVER['HTTP_REFERER'], $_SERVER['HTTP_HOST']) === false) {
		echo "you've been had.";
		trigger_error("off-site tried to vote on non-topic upload, referer: ".$_SERVER['HTTP_REFERER'], E_USER_WARNING);
		return;
	  }
	  
	  // quietly snub them if they are not an admin and they are trying to post to the changeblog
	  if(array_key_exists("submit", $_REQUEST) && $upload->id() == "211604" && me()->status() != "admin") {
	      return;
	  }
	  
		if( array_key_exists("submit", $_POST)) {
			$args = $_POST;
			if(array_key_exists("offensive", $args) && $args["offensive"] != "") $args["offensive"] = 1;
			if(array_key_exists("repost", $args) && $args["repost"] != "") $args["repost"] = 1;
			if(array_key_exists("subscribe", $args) && $args["subscribe"] != "") $args["subscribe"] = 1;
			$comment = core_postcomment($args);

			if(!is_object($comment)) {
				return;
			}

			if( array_key_exists("redirect", $_REQUEST) ) {
				header("Location: ".Link::upload($_REQUEST['fileid']));
				exit;
			}
			
			// redirecting to the same place prevents 'reload' from reposting the comment.
			header("Location: ".Link::comment($comment));
			exit;
		}
	}

	function head() {
		JS::add("/offensive/js/subscriptions.js");
		JS::add("/offensive/js/favico.js");
	}

	function head_post_js() {
		global $upload;
		// new comments arrive at the end of the thread, so only its last page,
		// which is also the only one with the comment form, shows them as
		// they're posted.
		if(commentPage()["hasNext"]) return;
		?>
		<script type="text/javascript">
			getSocket("<?php $t = new Token("realtime"); echo $t->tokenid(); ?>", function(socket) {
				var latest_comment;

				function isLatestCommentVisible() {
					var docViewTop = $(window).scrollTop();
					var docViewBottom = docViewTop + $(window).height();
					
                                        var offset = $(latest_comment).offset();
                                        if (!offset)
                                                return false;
                                        var elemTop = offset.top;
					var elemBottom = elemTop + $(latest_comment).height();
					
					return ((elemBottom >= docViewTop) && (elemTop <= docViewBottom)
					&& (elemBottom <= docViewBottom) &&  (elemTop >= docViewTop) );
				}

				var favicon = new Favico({
					animation: 'slide',
					bgColor: '#000033',
					textColor: '#ccccff'
				});

				var unread = 0;

				function handleEvent() {
					if (latest_comment && isLatestCommentVisible()) {
						latest_comment = null;
						unread = 0;
						favicon.badge(unread);
						$.ajax("/offensive/api.php/resetsubscription.json", {
							data: {
								threadid: '<?= $upload->id() ?>'
							}
						});
					}
					return true;
				}

				$(document).on('mousemove keydown DOMMouseScroll mousewheel mousedown touchstart touchmove', handleEvent);


				function commentExists(comment) {
					// comment.inc puts the comment id on the .entry element itself.
					return document.getElementById(String(comment.id)) !== null;
				}

				function addComment(comment) {
					if(commentExists(comment))
						return;
					var html = $(comment.html);
                                        // Find the entry by class: the template's top-level node layout has changed before.
                                        var entry = html.filter('.entry');
                                        if (entry.length == 0)
                                                entry = html.find('.entry');
                                        entry = entry.first();
					if($.inArray(comment.userid, me.squelched) != -1)
                                                entry.find('.comment_body').html('<div class="squelched">[ squelched ]</div>');
					// after the last comment: before the links under it, or the thumbnail
					var end = document.querySelector('#commentList > .entry ~ nav, #thumbnail');
					if (end)
						$(end).before(html);
					else
						$("#commentList").append(html);
                                        if (entry.length == 0)
                                                return;
                                        entry.hide();
					var currentScroll = $(window).scrollTop();
					var slideUp = $('textarea').is(':focus');
					var propTotals = {};
                                        latest_comment = entry;
                                        entry.animate({
    				"height": "show",
    				"marginTop": "show",
    				"marginBottom": "show",
    				"paddingTop": "show",
    				"paddingBottom": "show"
					}, {
						step: function(now, fx) {
							if (slideUp) {
								propTotals[fx.prop] = now;
								var scrollTo = currentScroll;
								$.each(propTotals, function(idx, change) { scrollTo += change; });
								$(window).scrollTop(scrollTo);
							}
						}
					});
				}

				var heldComments = [];
				var handleRealtimeComment = function(comment) {
					heldComments.push(comment);
				}

				socket.on('disconnect', function() {
	      	$("#comment_form").off('submit.realtime');
				});
					
	      socket.on('connect', function() {
	      	$("#comment_form").on('submit.realtime', function(e) {
	      		e.preventDefault();
	      		e.stopPropagation();
	      		var data = {
	      			vote: $('#comment_form input[name=vote]:checked').val() || "novote",
	      			comment: $("#comment_form textarea[name=comment]").val() || "",
	      			offensive: $("#comment_form #tmbo").attr("checked") ? "1" : "0",
	      			repost: $("#comment_form #repost").attr("checked") ? "1" : "0",
	      			subscribe: $("#comment_form #subscribe").attr("checked") ? "1" : "0",
	      			fileid: $("#comment_form #fileid").val()
	      		};

	      		$("#comment_form input:submit").prop('disabled', true);

	      		var request = $.post('/offensive/api.php/postcomment.php', data);
	      		request.done(function() {
	      			if(data.vote != "novote")
	      				$("#vote_controls").remove();
	      		  if(data.comment != '' || data.vote == "this is bad" || data.subscribe != "0")
	      				toggle_subscribe("subscribe", data.fileid, $("#subscribeLink"));
	      			$("#comment_form")[0].reset();
	      		});
	      		request.always(function() {
	      			$("#comment_form input:submit").prop('disabled', false);
	      		});
	      	});
	      });

				socket.on('comment', function(comment) {
					handleRealtimeComment(comment);
				});

				socket.emit('subscribe', '/uploads/<?= $upload->id() ?>');

				$.getJSON('/offensive/api.php/getcomments.json', {
					thread: <?= $upload->id() ?>,
					idmin: lastcomment,
					html: 1
				}, function(data) {
					data.forEach(addComment);
					heldComments.forEach(addComment);
					handleRealtimeComment = function(comment) {
						addComment(comment);
						if(comment.comment) {
							unread += 1;
							favicon.badge(unread);
						}
					}
				});
			});
		</script>
		<?php
	}
	
	function toggleThumbnailLink() {
		?><div style="margin:4px;font-weight:bold"><?php
			if(me()->getPref("thumbnails_in_comments") == 1) {
				?><a href="<?= Link::setPref("thumbnails_in_comments", 0) ?>">hide thumbnail</a><?php
			}
			else {
				?><a href="<?= Link::setPref("thumbnails_in_comments", 1) ?>">show thumbnail</a><?php
			}
		?></div><?php

	}

	// digits only, and few enough that arithmetic on it stays an int
	function isCount($value) {
		return is_string($value) && strlen($value) <= 9 && ctype_digit($value);
	}

	/*
	 * the page of the thread to show. ?limit= (a number or "all") overrides
	 * the viewer's comments_per_page preference, ?offset= is the index of the
	 * first comment shown, and ?comment= (from Link::comment) picks the page
	 * that comment is on.
	 */
	function commentPage() {
		global $upload;
		static $page = null;
		if($page !== null) return $page;

		$fromUrl = array_key_exists("limit", $_GET) && is_string($_GET["limit"]);
		$limit = $fromUrl ? $_GET["limit"] : me()->getPref("comments_per_page");
		if($limit === "all") {
			$limit = false;
		} else if(isCount($limit) && (int)$limit > 0) {
			$limit = (int)$limit;
		} else {
			// a bad ?limit= or preference gets the default
			$fromUrl = false;
			$limit = 250;
		}

		$offset = 0;
		if($limit !== false) {
			if(array_key_exists("comment", $_GET) && isCount($_GET["comment"])) {
				$comment = new Comment((int)$_GET["comment"]);
				if($comment->exists() && $comment->upload()->id() == $upload->id()) {
					$index = $upload->commentIndex($comment->id());
					$offset = $index - $index % $limit;
				}
			} else if(array_key_exists("offset", $_GET) && isCount($_GET["offset"])) {
				$offset = (int)$_GET["offset"];
			}
		}

		$total = false;
		if($limit !== false) {
			$total = $upload->commentCount();
			// past the end of the thread: show its last page
			if($offset >= $total && $total > 0) {
				$offset = ($total - 1) - ($total - 1) % $limit;
			}
		}
		$comments = $upload->getCommentRange($offset, $limit);
		if($total === false) $total = count($comments);
		$older = min($offset, $total);
		$newer = max(0, $total - $offset - count($comments));

		$page = array(
			"limit" => $limit,
			"limitFromUrl" => $fromUrl,
			"offset" => $offset,
			"total" => $total,
			"comments" => $comments,
			"older" => $older,
			"newer" => $newer,
			"hasPrev" => $older > 0,
			"hasNext" => $newer > 0,
		);
		return $page;
	}

	// a page of the thread starting at comment $offset, keeping ?limit=
	function pageLink($offset) {
		global $upload;
		$page = commentPage();
		$args = array();
		if($offset > 0) $args["offset"] = $offset;
		if($page["limitFromUrl"]) $args["limit"] = $page["limit"];
		return Link::thread($upload).(count($args) ? "&".http_build_query($args) : "");
	}

	/*
	 * the link that switches between pages and the whole thread: "view all"
	 * on a paged thread, "view paginated" on a whole thread that's too long
	 * for one page. null when the thread fits on one page.
	 */
	function viewToggle() {
		global $upload;
		$page = commentPage();
		$thread = Link::thread($upload);
		if($page["limit"] !== false) {
			if(!$page["hasPrev"] && !$page["hasNext"]) return null;
			return array($thread."&limit=all", "view all");
		}

		require("offensive/data/preferences.inc");
		$paged = me()->getPref("comments_per_page");
		$href = $thread;
		if($paged === "all") {
			// the preference says whole threads, so ask for pages explicitly
			$paged = $prefdefaults["comments_per_page"];
			$href .= "&limit=".$paged;
		}
		if($page["total"] <= (int)$paged) return null;
		return array($href, "view paginated");
	}

	/*
	 * above the comments, "first" and "<n> older" when there's a page before
	 * this one. below them, "<n> newer" and "latest" when there's one after.
	 * both have the view all / view paginated link. nothing when the thread
	 * fits on one page.
	 */
	function pageNav($top) {
		$page = commentPage();
		$links = array();
		if($top && $page["hasPrev"]) {
			$links[] = array(pageLink(0), "&#x21C8; first");
			$links[] = array(pageLink(max(0, $page["offset"] - $page["limit"])), "&uarr; ".$page["older"]." older");
		}
		if(!$top && $page["hasNext"]) {
			$last = $page["total"] - 1;
			$links[] = array(pageLink($page["offset"] + $page["limit"]), "&darr; ".$page["newer"]." newer");
			$links[] = array(pageLink($last - $last % $page["limit"]), "&#x21CA; latest");
		}
		$toggle = viewToggle();
		if($toggle) $links[] = $toggle;
		if(count($links) == 0) return;

		?><nav><?php
		foreach($links as $link) {
			?><a href="<?= htmlEscape($link[0]) ?>"><?= $link[1] ?></a><?php
		}
		?></nav><?php
	}

	function body() {
		global $upload, $uploader;

		$page = commentPage();
		$comments_exist = count($page["comments"]) > 0 || $page["hasPrev"];
	
		$comments_heading = "the dorks who came before you said:";
		$add_comment_heading = $comments_exist ? "and then you came along and were all:" : "you were first on the scene and were all:";

		$href = Link::upload($upload);
?>	

<div class="contentbox">
	<div class="blackbar"></div>
		<div class="heading"><?php
    // changeblog special header
	if($upload->id() == "211604") {
		?>
		changeblog
		<br/><span style="color:var(--link-user)">beat to fit. paint to match.</span><br/>
		<?php
	} else if( $upload->type() == 'topic' ) {
		$prefix = $uploader->status() == "admin" ? "" : "don't blame me,";

		echo htmlEscape($upload->filename());

		?><br/><span class="blame"><?= $prefix ?> <?= $uploader->htmlUsername() ?> started it.</span><br/>
<?php
	} else {
?>
		<a class="heading" href="<?= $href ?>"><?= $upload->htmlFilename() ?></a><br/><span class="blame">uploaded by <?= $uploader->htmlUsername() ?></span><br/>
<?php
	}	
?></div>
	
<?php
		global $activeTab;
		$activeTab = "discussions";
		tabs();
?>
	
		<div id="commentList" class="bluebox" style="text-align:left">	

<?php 

	$lastcomment = 0;
	if( $comments_exist > 0) { 
        // changeblog doesn't say anything demeaning about the people who posted.
        if($upload->id() != "211604") { 
			// one element, so it doesn't throw off the .entry:nth-child striping
			?><div><b><?= $comments_heading ?></b><label><input type="checkbox" id="link_previews" name="link_previews" aria-label="show link previews" /><span>show</span><span>hide</span> link previews</label><?php pageNav(true); ?></div><?php
        } else {
			pageNav(true);
		}

		// templates/comment.inc keeps the running tally in these globals
		global $numgood, $numbad;
		$numgood = 0;
		$numbad = 0;
		if($page["hasPrev"] && count($page["comments"])) {
			list($numgood, $numbad) = $upload->votesBefore($page["comments"][0]->id());
		}
		foreach($page["comments"] as $comment) {
			$lastcomment = $comment->id();
			require("offensive/templates/comment.inc");
		}
		pageNav(false);
	// end of "are there any comments"
	}
	
		// thumbnail
		if( $upload->type() != 'topic' && ($upload->type() != "audio" || $upload->thumb())) {
			?>
				<div id="thumbnail" style="background:var(--bg-content); border-top:1px solid var(--bg-heading)">
					<div style="float:right"><?php toggleThumbnailLink() ?></div>
					<?php
						if(me()->getPref("thumbnails_in_comments") == 1) {
							?>
							
							<a href="<?= $href ?>"
								<?php if($upload->filtered()) { ?>
									onMouseOver='changesrc("th<?= $upload->id()?>","<?= $upload->thumbURL() ?>")'
							 		onMouseOut='changesrc("th<?= $upload->id() ?>","/offensive/graphics/th-filtered.gif")'
								<?php } ?>
							><img name="th<?= $upload->id()?>" style="margin:12px;"
								src="<?= $upload->filtered()
									? "/offensive/graphics/th-filtered.gif" 
									: $upload->thumbURL() ?>" border="0"
							/></a>
							
							<?php
						}
					?>
					
				</div>
				<div style="clear:both">
				</div>
			<?php
		}

	?>
	
		

			</div>
                        <?php // outside #commentList so it doesn't throw off the .entry:nth-child striping ?>
                        <script>
                                lastcomment = <?php echo $lastcomment ?>;
                        </script>
			<?php if($page["limit"] !== false) { ?>
			<script>
				// a link from before threads had pages names its comment only in
				// the fragment, which never reaches the server: if the comment
				// isn't on this page, ask for the page it's on
				(function() {
					function findComment() {
						var id = location.hash.slice(1);
						if(!/^\d+$/.test(id) || document.getElementById(id)) return;
						var url = new URL(location.href);
						if(url.searchParams.get("comment") === id) return;
						url.searchParams.set("comment", id);
						url.searchParams.delete("offset");
						location.replace(url.href);
					}
					findComment();
					window.addEventListener("hashchange", findComment);
				})();
			</script>
			<?php } ?>
			<div class="heading">
				<div style="float:right">					
					<?php if( $upload->subscribed() ) { ?>
						<b><a id="subscribeLink" href="subscribe.php?un=1&fileid=<?= $upload->id() ?>" title="take this thread off my 'unread comments' watch list." class="orange">unsubscribe</a></b>
					<?php }
						else { ?>
						<b><a id="subscribeLink" href="subscribe.php?fileid=<?= $upload->id() ?>" title="watch this thread for new comments." class="orange">subscribe</a></b>
					<?php 	}
					?>
				</div>

					<div>
					<?php if ($upload->type() != "topic") { ?>
						<a class="heading" href="<?= Link::upload($upload) ?>"><?= $upload->htmlFilename(); ?></a><br/>
					<?php }
						else {
						 ?>&nbsp;<?php
						}
					?>
					</div>

			</div>
	</div>

<?php
    // changeblog shouldn't tease the users with a comment field, and the
    // form only goes at the end of the thread
    if($upload->canComment() && !$page["hasNext"]) {
?>

	<div class="contentbox">
		<div class="blackbar"></div>
			<div class="heading"><?= $add_comment_heading?></div>
			<div class="bluebox" style="text-align:center">			
			<a name="form"></a>
			<form method="POST" id="comment_form" action="<?php echo $_SERVER['SCRIPT_NAME']?>">

			
				<p>
					<input type="hidden" id="fileid" name="fileid" value="<?= $upload->id() ?>"/>
					<input type="hidden" name="c" value="comments"/>
					<textarea name="comment" rows="12" cols="64"></textarea>
				</p>
				


<?php 

	if($upload->canVote()) {
	
	?>	
	    <div id="vote_controls" style="text-align:left;margin-left:10%">
			<input type="radio" name="vote" value="novote" checked="checked" id="novote"/>
			<br/>			
			<input type="radio" name="vote" value="this is good" id="this_is_good"/>
			<label for="this_is_good">[ this is good ]</label><br/>
			<input type="radio" name="vote" value="this is bad" id="this_is_bad"/>
			<label for="this_is_bad">[ this is bad ]</label><br/>
		<br/>

		<input type="checkbox" name="offensive" value="omg" id="tmbo"/>
		<label for="tmbo">[ this might be offensive ]</label><br/>
		
		<input type="checkbox" name="repost" value="police" id="repost"/>
		<label for="repost">[ this is a repost ]</label><br/>

		<br/>

		<input type="checkbox" name="subscribe" value="subscribe" id="subscribe"/>
		<label for="subscribe">[ subscribe ]</label><br/>


</div>

	<?php

	}
?>

						
				<p>
					<input type="submit" name="submit" value="go"/>
				</p>
				
				</form>
			</div>
			<div class="blackbar"></div>
		</div>
			

<?php
    }
}
?>
